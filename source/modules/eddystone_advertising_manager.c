#include "eddystone_advertising_manager.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "ble_ecs.h"
#include "eddystone.h"
#include "eddystone_registration_ui.h"
#include "eddystone_app_config.h"
#include "eddystone_security.h"
#include "eddystone_adv_slot.h"
#include "app_timer.h"
#include "app_error.h"
#include "endian_convert.h"
#include "bsp.h"
#include "eddystone_tlm_manager.h"
#include "debug_config.h"

static ble_gap_adv_params_t m_non_conn_adv_params;               /**< Parameters to be passed to the stack when starting advertising in non-connectable mode. */
static ble_gap_adv_params_t m_conn_adv_params;                   /**< Parameters to be passed to the stack when starting advertising in connectable mode. */

#ifdef ADV_DEBUG
    #include "SEGGER_RTT.h"
    #define DEBUG_PRINTF SEGGER_RTT_printf
#else
    #define DEBUG_PRINTF(...)
#endif

APP_TIMER_DEF(m_eddystone_adv_interval_timer);
APP_TIMER_DEF(m_eddystone_adv_slot_timer);
APP_TIMER_DEF(m_eddystone_etlm_cycle_timer);

static bool m_is_connectable_adv = false;
static bool m_is_connected       = false;
static uint8_t m_ecs_uuid_type = 0;

//Struct to keep track of the pairing between eTLM and EIDs
typedef struct
{
  uint8_t eid_slot_counter;                         /**<which element in the eid_positions array is the eTLM current paired with */
  uint8_t eid_positions[APP_MAX_EID_SLOTS];         /**<see param[out] of @ ref eddystone_adv_slot_num_of_current_eids() */
} eddystone_etlm_adv_counter_t;

/**@brief Struct of all advertising timing related intervals that control timers for advertising*/
typedef struct
{
    uint16_t adv_intrvl;                /**<the interval between the advertisings of a particular slot */
    uint16_t slot_slot_interval;        /**<the interval between the advertising of two adjacent slots */
    uint16_t etlm_etlm_interval;        /**<the interval between the advertising of two adjacent eTLM frames, each with its own EID pairing */
} eddystone_adv_manager_intervals_t;


static eddystone_adv_manager_intervals_t m_intervals;
static eddystone_etlm_adv_counter_t      m_etlm_adv_counter;
static uint8_t m_currently_configured_slots[APP_MAX_ADV_SLOTS] = {0};
static uint8_t m_temporary_slot_no;

//Forward Declarations
static void slots_advertising_start(void);
static void advertising_init(uint8_t slot);
static void all_advertising_halt(void);
static void adv_interval_timer_start(void);
static void intervals_calculate(void);
static void adv_slot_timer_start(void);
static void fetch_adv_data_from_slot( uint8_t slot, uint8_array_t * p_eddystone_data_array );

/**@brief Function for starting advertising of the eddystone beacon.
 * @param[in]   conn  connectable or non-connectable
 */
static void eddystone_ble_advertising_start(eddystone_ble_adv_connectable_t conn)
{
    uint32_t err_code;

    eddystone_tlm_manager_adv_cnt_add(1);

    switch (conn)
    {
        case EDDYSTONE_BLE_ADV_CONNECTABLE_FALSE:
            err_code = sd_ble_gap_adv_start(&m_non_conn_adv_params);
            break;
        case EDDYSTONE_BLE_ADV_CONNECTABLE_TRUE:
            LEDS_ON(1 << LED_3);
            bsp_indication_set(BSP_INDICATE_ADVERTISING);
            DEBUG_PRINTF(0,"Connectable ADV... \r\n",0);
            err_code = sd_ble_gap_adv_start(&m_conn_adv_params);
            break;
    }

    if (err_code != NRF_ERROR_BUSY && err_code != NRF_SUCCESS)
    {
        APP_ERROR_CHECK(err_code);
    }
}

/**@brief Function for stopping all advertising and all running timers */
void all_advertising_halt(void)
{
    sd_ble_gap_adv_stop();
    app_timer_stop(m_eddystone_adv_interval_timer);
    app_timer_stop(m_eddystone_adv_slot_timer);
    app_timer_stop(m_eddystone_etlm_cycle_timer);
}

/**@brief Function for starting connectable advertising of the eddystone beacon to register it
 * @details  Used by the eddystone_registration_ui module as a callback when the registration button is pressed.
 */
static void eddystone_ble_registr_adv_cb(void)
{
    if (m_is_connectable_adv != true && m_is_connected == false)
    {
        all_advertising_halt();

        uint32_t      err_code;
        ble_advdata_t adv_data;
        ble_advdata_t scrsp_data;
        ble_uuid_t    adv_uuids[] = {{EDDYSTONE_UUID, BLE_UUID_TYPE_BLE}};
        ble_uuid_t    scrp_uuids[] = {{BLE_UUID_ECS_SERVICE, m_ecs_uuid_type}};

        uint8_array_t eddystone_data_array;                             // Array for Service Data structure.

        eddystone_data_array.size = 0;
        eddystone_data_array.p_data = NULL;

        ble_advdata_service_data_t service_data;                        // Structure to hold Service Data.
        service_data.service_uuid = APP_EDDYSTONE_UUID;                 // Eddystone UUID to allow discoverability on iOS devices.
        service_data.data = eddystone_data_array;                                       // Array for service advertisement data.

        // Build and set advertising data.
        memset(&adv_data, 0, sizeof(adv_data));

        adv_data.name_type               = BLE_ADVDATA_NO_NAME;
        adv_data.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
        adv_data.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
        adv_data.uuids_complete.p_uuids  = adv_uuids;
        adv_data.p_service_data_array    = &service_data;                // Pointer to Service Data structure.
        adv_data.service_data_count      = 1;

        memset(&scrsp_data, 0, sizeof(scrsp_data));
        scrsp_data.name_type               = BLE_ADVDATA_FULL_NAME;
        scrsp_data.include_appearance      = false;
        scrsp_data.uuids_complete.uuid_cnt = sizeof(scrp_uuids) / sizeof(scrp_uuids[0]);
        scrsp_data.uuids_complete.p_uuids  = scrp_uuids;

        err_code = ble_advdata_set(&adv_data, &scrsp_data);
        APP_ERROR_CHECK(err_code);

        memset(&m_conn_adv_params, 0, sizeof(m_conn_adv_params));

        m_conn_adv_params.type           = BLE_GAP_ADV_TYPE_ADV_IND;
        m_non_conn_adv_params.p_peer_addr = NULL;                                // Undirected advertisement.
        m_non_conn_adv_params.fp          = BLE_GAP_ADV_FP_ANY;
        m_conn_adv_params.interval       = MSEC_TO_UNITS(DEFAULT_CONNECTABLE_ADV_INTERVAL_MS, UNIT_0_625_MS);
        m_conn_adv_params.timeout        = APP_CFG_CONNECTABLE_ADV_TIMEOUT;

        m_is_connectable_adv = true;
        eddystone_ble_advertising_start(EDDYSTONE_BLE_ADV_CONNECTABLE_TRUE);
    }
}

/**@brief Function for handling events coming from the S132 SoftDevice
 * @param[in]   p_ble_evt   Pointer to the ble event
 */
void eddystone_advertising_manager_on_ble_evt( ble_evt_t * p_ble_evt )
{
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            bsp_indication_set(BSP_INDICATE_IDLE);
            LEDS_ON(1<<LED_2);
            LEDS_OFF(1<<LED_3);
            m_is_connectable_adv = false;
            m_is_connected = true;
            slots_advertising_start();
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            bsp_indication_set(BSP_INDICATE_IDLE);
            LEDS_OFF(1<<LED_2);
            LEDS_OFF(1<<LED_3);
            m_is_connected = false;
            m_is_connectable_adv = false;
            all_advertising_halt();

            adv_interval_timer_start();
            //Essentially gives 1 advertising interval's time for flash to write
            break;

        case BLE_GAP_EVT_TIMEOUT:
            if (p_ble_evt->evt.gap_evt.params.timeout.src == BLE_GAP_TIMEOUT_SRC_ADVERTISING)
            {
                //When connectable advertising times out, switch back to Non-connectable
                bsp_indication_set(BSP_INDICATE_IDLE);
                LEDS_OFF(1<<LED_3);
                m_is_connectable_adv = false;
                slots_advertising_start();
            }
            break;

        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
            if (p_ble_evt->evt.gatts_evt.params.authorize_request.type == BLE_GATTS_AUTHORIZE_TYPE_WRITE)
            {
                DEBUG_PRINTF(0,"Stop Advertising For A bit!! \r\n",0);
                all_advertising_halt();
                adv_interval_timer_start();
            }
        default:
            // No implementation needed.
            break;
    }
}

/**@brief Function for handling events coming from the S132 SoftDevice
 * @param[in]   slot                            Slot index
 * @param[out]   p_eddystone_data_array         buffer array to which the adv data is retrieved
 */
static void fetch_adv_data_from_slot( uint8_t slot, uint8_array_t * p_eddystone_data_array )
{
    eddystone_adv_slot_params_t eddystone_adv_slot_params;
    eddystone_adv_slot_params_get(slot, &eddystone_adv_slot_params);

    sd_ble_gap_tx_power_set(eddystone_adv_slot_params.radio_tx_pwr);

    switch (eddystone_adv_slot_params.frame_type)
    {
        case EDDYSTONE_FRAME_TYPE_UID:
            p_eddystone_data_array->p_data = (uint8_t *) &(eddystone_adv_slot_params.p_adv_frame->uid);
            p_eddystone_data_array->size = sizeof(eddystone_uid_frame_t);
            break;
        case EDDYSTONE_FRAME_TYPE_URL:
            p_eddystone_data_array->p_data = (uint8_t *) &(eddystone_adv_slot_params.p_adv_frame->url);
            p_eddystone_data_array->size = eddystone_adv_slot_params.url_frame_length;
            break;
        case EDDYSTONE_FRAME_TYPE_TLM:
            //If there are EIDs, broadcast eTLM, else just TLM
            if (eddystone_adv_slot_num_of_current_eids(NULL) != 0)
            {
                //The current EIK slot that is paired with the current eTLM slot
                uint8_t eik_pair_slot = m_etlm_adv_counter.eid_positions[m_etlm_adv_counter.eid_slot_counter];

                eddystone_tlm_manager_etlm_get(eik_pair_slot, &(eddystone_adv_slot_params.p_adv_frame->etlm));

                p_eddystone_data_array->p_data = (uint8_t *) &(eddystone_adv_slot_params.p_adv_frame->etlm);
                p_eddystone_data_array->size = sizeof(eddystone_etlm_frame_t);
            }
            //Just plain TLM
            else
            {
                eddystone_tlm_manager_tlm_get(&(eddystone_adv_slot_params.p_adv_frame->tlm));
                p_eddystone_data_array->p_data = (uint8_t *) &(eddystone_adv_slot_params.p_adv_frame->tlm);
                p_eddystone_data_array->size = sizeof(eddystone_tlm_frame_t);
            }
            break;
        case EDDYSTONE_FRAME_TYPE_EID:
            p_eddystone_data_array->p_data = (uint8_t *) &(eddystone_adv_slot_params.p_adv_frame->eid);
            p_eddystone_data_array->size = sizeof(eddystone_eid_frame_t);
            break;
        default:
            __NOP();
            break;
    }
}

/**@brief Function for initializing the advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
static void advertising_init( uint8_t slot )
{
    uint32_t      err_code;
    ble_advdata_t adv_data;
    ble_uuid_t    adv_uuids[] = {{EDDYSTONE_UUID, BLE_UUID_TYPE_BLE}};

    uint8_array_t eddystone_data_array;                             // Array for Service Data structure.

    fetch_adv_data_from_slot(slot,&eddystone_data_array);

    ble_advdata_service_data_t service_data;                        // Structure to hold Service Data.
    service_data.service_uuid = APP_EDDYSTONE_UUID;                 // Eddystone UUID to allow discoverability on iOS devices.
    service_data.data = eddystone_data_array;                       // Array for service advertisement data.

    // Build and set advertising data.
    memset(&adv_data, 0, sizeof(adv_data));

    adv_data.name_type               = BLE_ADVDATA_NO_NAME;
    adv_data.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    adv_data.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
    adv_data.uuids_complete.p_uuids  = adv_uuids;
    adv_data.p_service_data_array    = &service_data;                // Pointer to Service Data structure.
    adv_data.service_data_count      = 1;

    //DEBUG_PRINTF(0, "Slot [%d] - Service Data Size: %d \r\n", slot, service_data.data.size);

    err_code = ble_advdata_set(&adv_data, NULL);
    APP_ERROR_CHECK(err_code);

    // Initialize advertising parameters (used when starting advertising).
    memset(&m_non_conn_adv_params, 0, sizeof(m_non_conn_adv_params));

    /*Non-connectable*/
    m_non_conn_adv_params.type        = BLE_GAP_ADV_TYPE_ADV_NONCONN_IND;
    m_non_conn_adv_params.p_peer_addr = NULL;                                // Undirected advertisement.
    m_non_conn_adv_params.fp          = BLE_GAP_ADV_FP_ANY;
    m_non_conn_adv_params.interval    = MSEC_TO_UNITS(m_intervals.adv_intrvl, UNIT_0_625_MS);
    m_non_conn_adv_params.timeout     = APP_CFG_NON_CONN_ADV_TIMEOUT;
}

/**@brief Function for starting the advertising interval timer.*/
static void adv_interval_timer_start(void)
{
    ret_code_t err_code;
    intervals_calculate();
    err_code = app_timer_start(m_eddystone_adv_interval_timer,APP_TIMER_TICKS(m_intervals.adv_intrvl,APP_TIMER_PRESCALER), NULL);
    APP_ERROR_CHECK(err_code);
}

static void intervals_calculate(void)
{
    //Buffers to hardcode a small shortening of the intervals just in case of any delays
    //between all the running timer interrupts + encyrption processes of eTLMs so that each slot
    //has enough time to advertise before the next advertising interval comes around

    const uint8_t etlm_dist_buffer_ms = 25;
    const uint8_t slot_dist_buffer_ms = 25;

    /**@note From current measurements at Nordic we can see that eTLM encryption takes about 140 ms on the NRF52
    Which means that the delay after the timer interrupt fires to advertise and the actual eTLM advertisement is 140 ms */

    const uint16_t SLOT_INTERVAL_LIMIT = 500; //Minimum interval between two slots in ms
    const uint16_t ETLM_INTERVAL_LIMIT = 300;  //Minimum interval between two eTLMs on the same slot in ms, must be > 140 ms

    //See if any slot is configured at all
    uint8_t no_of_currently_configed_slots = eddystone_adv_slot_num_of_configured_slots(m_currently_configured_slots);
    DEBUG_PRINTF(0,"Number of Configured Slots: %d \r\n", no_of_currently_configed_slots);

    //Gets slot 0's advertising interval since only global advertising interval is supported currently
    eddystone_adv_slot_params_t adv_slot_0_params;
    eddystone_adv_slot_params_get(0, &adv_slot_0_params);
    m_intervals.adv_intrvl = adv_slot_0_params.adv_intrvl;

    //Can happen when flash R/W for storing/loading slot configs did not behave as expected
    //which can crash the app_timer_start
    //TODO: might need a better error handling strategy for this...
    if (m_intervals.adv_intrvl == 0)
    {
        m_intervals.adv_intrvl = 1000;
    }

    if (no_of_currently_configed_slots == 0)
    {
        m_intervals.slot_slot_interval = 0;
        m_intervals.etlm_etlm_interval = 0;
    }
    else
    {
        //Slot-Slot Interval
        m_intervals.slot_slot_interval = (m_intervals.adv_intrvl/(no_of_currently_configed_slots)) - slot_dist_buffer_ms;
        if (m_intervals.slot_slot_interval < SLOT_INTERVAL_LIMIT)
        {
            ble_ecs_adv_intrvl_t adjusted_interval = (SLOT_INTERVAL_LIMIT + slot_dist_buffer_ms)*no_of_currently_configed_slots;
            m_intervals.adv_intrvl = adjusted_interval;

            DEBUG_PRINTF(0,"1 - ADV INTERVAL ADJUSTED BY ADV MGR: %d \r\n", adjusted_interval);
            adjusted_interval = BYTES_SWAP_16BIT(adjusted_interval);
            eddystone_adv_slot_adv_intrvl_set(0, &adjusted_interval, true);
            m_intervals.slot_slot_interval = m_intervals.adv_intrvl/no_of_currently_configed_slots - slot_dist_buffer_ms;
            DEBUG_PRINTF(0,"Slot-Slot Interval: %d \r\n", m_intervals.slot_slot_interval );
        }

        //eTLM-eTLM interval
        uint8_t no_of_eid_slots = eddystone_adv_slot_num_of_current_eids(NULL);

        if (no_of_eid_slots != 0)
        {
            m_intervals.etlm_etlm_interval = (m_intervals.slot_slot_interval/no_of_eid_slots) - etlm_dist_buffer_ms;

            if (m_intervals.etlm_etlm_interval < ETLM_INTERVAL_LIMIT)
            {
                m_intervals.etlm_etlm_interval = ETLM_INTERVAL_LIMIT;
                m_intervals.slot_slot_interval = (m_intervals.etlm_etlm_interval + etlm_dist_buffer_ms)*no_of_eid_slots - slot_dist_buffer_ms;

                ble_ecs_adv_intrvl_t adjusted_interval = (m_intervals.slot_slot_interval + slot_dist_buffer_ms)*no_of_currently_configed_slots;
                m_intervals.adv_intrvl = adjusted_interval;

                DEBUG_PRINTF(0,"2 - ADV INTERVAL ADJUSTED BY ADV MGR: %d \r\n", adjusted_interval);
                adjusted_interval = BYTES_SWAP_16BIT(adjusted_interval);
                eddystone_adv_slot_adv_intrvl_set(0, &adjusted_interval, true);

                DEBUG_PRINTF(0,"eTLM-eTLM Interval: %d \r\n", m_intervals.etlm_etlm_interval );
                DEBUG_PRINTF(0,"Slot-Slot Interval: %d \r\n", m_intervals.slot_slot_interval );
            }
        }
        else
        {
            m_intervals.etlm_etlm_interval = 0;
        }
    }
}

/**@brief Function for updating the etlm_cycle_timer
 * @param[in] slot_no    the slot index of the current eTLM slot
 */
static void etlm_cycle_timer_start(uint8_t slot_no)
{
    ret_code_t err_code;
    m_temporary_slot_no = slot_no;
    if (m_intervals.etlm_etlm_interval != 0)
    {
        err_code = app_timer_start(m_eddystone_etlm_cycle_timer, APP_TIMER_TICKS(m_intervals.etlm_etlm_interval, APP_TIMER_PRESCALER), NULL);
        APP_ERROR_CHECK(err_code);
    }
}

/**@brief Function for advertising an eTLM frame
 * @param[in] slot_no    the slot index of the current eTLM slot
 */
static void etlm_adv(uint8_t slot_no)
{
    DEBUG_PRINTF(0,"eTLM-EIK [%d] \r\n", m_etlm_adv_counter.eid_positions[m_etlm_adv_counter.eid_slot_counter]);
    advertising_init(slot_no);
    eddystone_ble_advertising_start(EDDYSTONE_BLE_ADV_CONNECTABLE_FALSE);
    //Increment the eid-pair counter so the eTLM frame can be paired with the next EID frame.
    m_etlm_adv_counter.eid_slot_counter++;
}

/**@brief Timeout handler for the etlm_cycle_timer*/
static void etlm_cycle_timeout(void * p_context)
{
    sd_ble_gap_adv_stop();

    if (m_etlm_adv_counter.eid_slot_counter >= eddystone_adv_slot_num_of_current_eids(NULL))
    {
        m_etlm_adv_counter.eid_slot_counter = 0;
        app_timer_stop(m_eddystone_etlm_cycle_timer);
    }
    else
    {
        etlm_adv(m_temporary_slot_no);
        //this will incremement eid_slot_counter
    }
}

/**@brief Timeout handler for the adv_slot_timer*/
static void adv_slot_timeout(void * p_context)
{
    static uint8_t slot_counter = 0;
    uint8_t slot_no = m_currently_configured_slots[slot_counter];

    m_etlm_adv_counter.eid_slot_counter = 0;

    if(slot_no != 0xFF)
    {
    sd_ble_gap_adv_stop();

        static uint8_t tick_tock = 0;
        tick_tock++;
        if (tick_tock % 2 == 0)
        {
            LEDS_ON(1<<LED_1);
        }
        else
        {
            LEDS_OFF(1<<LED_1);
        }

        eddystone_adv_slot_params_t adv_slot_params;
        eddystone_adv_slot_params_get(slot_no, &adv_slot_params);

        if (eddystone_adv_slot_is_configured(slot_no) && !m_is_connectable_adv)
        {
            DEBUG_PRINTF(0,"Slot [%d] - frame type: 0x%02x \r\n", slot_no, adv_slot_params.frame_type);

            if ((adv_slot_params.frame_type == EDDYSTONE_FRAME_TYPE_TLM)
                && eddystone_adv_slot_num_of_current_eids(m_etlm_adv_counter.eid_positions) != 0)
            {
                // DEBUG_PRINTF(0, "EID positions ", 0);
                // for (uint8_t i = 0; i < sizeof(m_etlm_adv_counter.eid_positions); i++)
                // {
                //     DEBUG_PRINTF(0, "0x%02x, ", m_etlm_adv_counter.eid_positions[i]);
                // }
                // DEBUG_PRINTF(0, "\r\n", 0);
                etlm_cycle_timer_start(slot_no);
                etlm_cycle_timeout(NULL);
                //Spoof a timeout to advertisie right away
            }
            else
            {
                //Regular TLM slot
                advertising_init(slot_no);
                eddystone_ble_advertising_start(EDDYSTONE_BLE_ADV_CONNECTABLE_FALSE);
            }

        }
    }
    slot_counter++;
    if (slot_counter >= eddystone_adv_slot_num_of_configured_slots(m_currently_configured_slots))
    {
        DEBUG_PRINTF(0,"End of Slots: \r\n");
        slot_counter = 0;
    }
    else
    {
        adv_slot_timer_start();
    }
}

/**@brief Function for starting the adv_slot_timer with the updated interval*/
static void adv_slot_timer_start(void)
{
    ret_code_t err_code;
    if (m_intervals.slot_slot_interval != 0)
    {
        err_code = app_timer_start(m_eddystone_adv_slot_timer,APP_TIMER_TICKS(m_intervals.slot_slot_interval,APP_TIMER_PRESCALER), NULL);
        APP_ERROR_CHECK(err_code);
    }
}

/**@brief Timeout handler for the adv_interval_timer*/
static void adv_interval_timeout(void * p_context)
{
    m_etlm_adv_counter.eid_slot_counter = 0;
    slots_advertising_start();
}

/**@brief Function for starting to advertise all slots with timer interval control */
static void slots_advertising_start(void)
{
    adv_interval_timer_start(); //this must start first, since it calculates parameters that the adv_slot_timer depends on
    adv_slot_timeout(NULL);     //Spoof a timeout right away so the actual advertising can begin immediately
}

/**@brief Function for initializing all required timers
 * @details The timer structure is illustrated below
 *          |-----------------------------|-----------------------------|  m_eddystone_adv_interval_timer (corresponds to the global advertising interval set in the slot)
 *          eTLM-----EID1-----EID2        eTLM-----EID1-----EID2           m_eddystone_adv_slot_timer (interval is the division of the advertising interval by the no. of slots configured)
 *          EIK1-EIK2                     EIK1-EIK2                        m_eddystone_etlm_cycle_timer (Cycles through each existing EID's EIK and pairs it with the eTLM)
 */
static void timers_init(void)
{
    ret_code_t err_code;

    //SINGLE SHOT because the advertising interval might be changed by the user, so the timer
    //needs to be restarted every time with the latest interval after a timeout
    err_code = app_timer_create(&m_eddystone_adv_interval_timer,
                     APP_TIMER_MODE_SINGLE_SHOT,
                     adv_interval_timeout);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_eddystone_adv_slot_timer,
                     APP_TIMER_MODE_SINGLE_SHOT,
                     adv_slot_timeout);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_eddystone_etlm_cycle_timer,
                     APP_TIMER_MODE_REPEATED,
                     etlm_cycle_timeout);
    APP_ERROR_CHECK(err_code);
}

void eddystone_advertising_manager_init( uint8_t ecs_uuid_type )
{
    uint32_t err_code;
    m_ecs_uuid_type = ecs_uuid_type;
    err_code = eddystone_registration_ui_init(eddystone_ble_registr_adv_cb);
    APP_ERROR_CHECK(err_code);
    timers_init();

    m_etlm_adv_counter.eid_slot_counter = 0;
    memset(m_etlm_adv_counter.eid_positions, 0xFF, sizeof(m_etlm_adv_counter.eid_positions));

    err_code = eddystone_tlm_manager_init();
    APP_ERROR_CHECK(err_code);

    slots_advertising_start();
    DEBUG_PRINTF(0,"Advertising Manager Init. \r\n");
}
