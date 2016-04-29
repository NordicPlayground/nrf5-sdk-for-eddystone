#include "eddystone_tlm_manager.h"
#include "eddystone_app_config.h"
#include "eddystone_security.h"
#include "app_error.h"
#include "app_timer.h"
#include "endian_convert.h"
#include <string.h>
#include "debug_config.h"

#ifdef TLM_DEBUG
    #include "SEGGER_RTT.h"
    #define DEBUG_PRINTF SEGGER_RTT_printf
#else
    #define DEBUG_PRINTF(...)
#endif

#ifdef TLM_DEBUG
uint8_t ascii_table[] = {
'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
};
// Helper function for pretty printing
static void print_array(uint8_t *p_data, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
    {
        uint8_t h,l;
        SEGGER_RTT_WriteString(0, "0x");
        h = ascii_table[((p_data[i] & 0xF0)>>4)];
        l = ascii_table[(p_data[i] & 0x0F)];
        SEGGER_RTT_Write(0, (char *)&h, 1);
        SEGGER_RTT_Write(0, (char *)&l, 1);
        SEGGER_RTT_WriteString(0, " ");
    }
    SEGGER_RTT_WriteString(0, "\r\n");
}
#endif

static eddystone_tlm_frame_t m_tlm =
{
    .frame_type = EDDYSTONE_FRAME_TYPE_TLM,
    .version    = EDDYSTONE_TLM_VERSION_TLM,
    //the rest are zeros by default
};

#define  NS_PER_TICK             (1+APP_TIMER_PRESCALER)*30517
#define  RTC1_TICKS_MAX          16777216

/**@brief Function for updating the ADV_SEC field of TLM*/
static void eddystone_tlm_update_time(void)
{
    static uint32_t previous_tick = 0;
    uint32_t        current_tick = 0;
    uint32_t        tick_diff;
    uint32_t        ms = 0;
    static uint32_t ms_remainder = 0;
    static uint32_t ns_remainder = 0;
    static uint32_t le_time = 0; //Little endian time
    uint32_t        be_time = 0; //Big endian

    APP_ERROR_CHECK(app_timer_cnt_get(&current_tick));

    if (current_tick > previous_tick)
    {
        //ticks since last update
        tick_diff = current_tick - previous_tick;
    }
    else if (current_tick < previous_tick)
    {
        //RTC counter has overflown
        tick_diff = current_tick + (RTC1_TICKS_MAX - previous_tick);
    }

    uint32_t ns;
    const uint32_t NS_PER_MS = 1000000;

    ns = tick_diff*NS_PER_TICK;
    ms = ns/NS_PER_MS;
    ns_remainder += ns % NS_PER_MS;

    if (ns_remainder >= NS_PER_MS)
    {
        ms += ns_remainder/NS_PER_MS;
        ns_remainder = ns_remainder % NS_PER_MS;
    }

    const uint32_t RESOLUTION = 100;
    //The eddystone spec requires 100 ms resolution, so only update the time when ms > 100
    if (ms >= RESOLUTION)
    {
        ms_remainder += ms % RESOLUTION;
        if (ms_remainder >= RESOLUTION)
        {
            ms += ms_remainder/RESOLUTION;
            ms_remainder = ms_remainder % RESOLUTION;
        }
        //For very first function call, ms will be >> 100, since previous_tick = 0. So only
        //incrememnt the time by 1.
        if (le_time == 0)
        {
            le_time++;
        }
        else
        {
            le_time += ((ms)/RESOLUTION);
        }
         be_time = BYTES_REVERSE_32BIT(le_time);
         memcpy(m_tlm.sec_cnt, &be_time, EDDYSTONE_TLM_SEC_CNT_LENGTH);
    }
    else
    {
        ms_remainder += ms;
    }
    previous_tick = current_tick;
}

ret_code_t eddystone_tlm_manager_init(void)
{

    eddystone_tlm_update_time(); //Increment the time right way during module init.
    return NRF_SUCCESS;
}

/**@brief Function for updating the TEMP field of TLM*/
static void eddystone_tlm_update_temp(void)
{
    ret_code_t err_code;
    const uint8_t  TLM_TEMP_INTERVAL = 30;
    static uint8_t temp_counter = TLM_TEMP_INTERVAL - 1;

    //No need to get the temperature every time a TLM is broadcast since temperature
    //is assumed to be pretty constant. Thus temperature is sampled every TLM_TEMP_INTERVAL times a
    //TLM is broadcast
    temp_counter++;

    if(temp_counter % TLM_TEMP_INTERVAL == 0)
    {
        int32_t temp;                       // variable to hold temp reading
        err_code = sd_temp_get(&temp);      // get new temperature
        APP_ERROR_CHECK(err_code);          // report errors if any
        int16_t temp_new = (int16_t) temp;  // convert from int32_t to int16_t
        m_tlm.temp[0] = (uint8_t)((temp_new >> 2) & 0xFFUL); // Right-shift by two to remove decimal part
        m_tlm.temp[1] = (uint8_t)((temp_new << 6) & 0xFFUL); // Left-shift 6 to get fractional part with 0.25 degrees C resolution
        temp_counter = 0;                   // reset temp counter
    }
}

void eddystone_tlm_manager_tlm_get(eddystone_tlm_frame_t * p_tlm_frame)
{
    eddystone_tlm_update_time();
    eddystone_tlm_update_temp();
    memcpy(p_tlm_frame, &m_tlm, sizeof(eddystone_tlm_frame_t));
}

void eddystone_tlm_manager_etlm_get( uint8_t eik_pair_slot, eddystone_etlm_frame_t * p_etlm_frame)
{
    uint8_t tlm[EDDYSTONE_ETLM_LENGTH] = {0};
    eddystone_tlm_manager_tlm_get((eddystone_tlm_frame_t*)tlm);
    eddystone_security_tlm_to_etlm(eik_pair_slot, (eddystone_tlm_frame_t*)tlm, p_etlm_frame);

    #ifdef TLM_DEBUG
    DEBUG_PRINTF(0,"TLM: ", 0);
    print_array(tlm, EDDYSTONE_TLM_LENGTH);
    DEBUG_PRINTF(0,"eTLM: ", 0);
    print_array((uint8_t*)(p_etlm_frame), EDDYSTONE_ETLM_LENGTH);
    #endif
}

void eddystone_tlm_manager_adv_cnt_add(uint8_t n)
{
    static uint32_t le_adv_cnt = 0; //little endian
    uint32_t        be_adv_cnt = 0; //big endian

    le_adv_cnt += n;
    be_adv_cnt = BYTES_REVERSE_32BIT(le_adv_cnt);

    memcpy(m_tlm.adv_cnt, (uint8_t*)(&be_adv_cnt), EDDYSTONE_TLM_ADV_CNT_LENGTH);
}
