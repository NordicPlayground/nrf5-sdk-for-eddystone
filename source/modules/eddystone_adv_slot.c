#include "eddystone_adv_slot.h"
#include "eddystone_app_config.h"
#include "nrf_error.h"
#include "ble_gap.h"
#include "endian_convert.h"
#include "eddystone_security.h"
#include "macros_common.h"
#include "eddystone_flash.h"
#include "eddystone_tlm_manager.h"
#include "debug_config.h"
#include "app_scheduler.h"
#include <stdint.h>
#include <string.h>

#ifdef SLOT_DEBUG
    #include "SEGGER_RTT.h"
    #include "print_array.h"
    #define DEBUG_PRINTF SEGGER_RTT_printf
    #define PRINT_ARRAY  print_array
#else
    #define DEBUG_PRINTF(...)
    #define PRINT_ARRAY(...)
#endif

//Forward Declaration:
uint32_t eddystone_adv_slot_adv_frame_set(uint8_t slot_no);
static void eddystone_adv_frame_set_scheduler_evt( void * p_event_data, uint16_t event_size );
static void eddystone_adv_slot_load_from_flash( uint8_t slot_no );

static eddystone_adv_slot_t m_slots[APP_MAX_ADV_SLOTS];

void eddystone_adv_slots_init( ble_ecs_init_t * p_ble_ecs_init )
{
    ret_code_t err_code;
    eddystone_flash_flags_t flash_flags;
    memset(&flash_flags, 0, sizeof(eddystone_flash_flags_t));

    //Read the flash flags to see if there are any previously stored slot configs
    err_code = eddystone_flash_access_flags(&flash_flags, EDDYSTONE_FLASH_ACCESS_READ);
    APP_ERROR_CHECK(err_code);

    uint32_t pending_ops = eddystone_flash_num_pending_ops();
    while (pending_ops != 0)
    {
        pending_ops = eddystone_flash_num_pending_ops();
    }
    DEBUG_PRINTF(0, "Flash Flags: \r\n",0);
    PRINT_ARRAY((uint8_t *)&flash_flags, sizeof(eddystone_flash_flags_t));
    //No previous configs, set default configs
    if (flash_flags.factory_state)
    {
        if(flash_flags.factory_state != 1 && flash_flags.factory_state != 0xFF)
        {
            APP_ERROR_CHECK(NRF_ERROR_INVALID_PARAM);
            //sanity check
        }
        for (uint8_t i = 0; i < APP_MAX_ADV_SLOTS; i++)
        {
            if (i == 0)
            {
                m_slots[i].slot_no = i;
                m_slots[i].adv_intrvl = p_ble_ecs_init->p_init_vals->adv_intrvl;
                m_slots[i].radio_tx_pwr = p_ble_ecs_init->p_init_vals->radio_tx_pwr;
                m_slots[i].frame_write_buffer[0] = p_ble_ecs_init->p_init_vals->rw_adv_slot.frame_type;

                //Copy length corresponds to the length of JUST the data in the frame, excluding frame type
                uint16_t copy_length = p_ble_ecs_init->p_init_vals->rw_adv_slot.char_length - 1;
                ret_code_t err_code;
                //If not a TLM frame
                if (copy_length > 0) //copy_length would be 0 for a TLM frame
                {
                    memcpy((m_slots[i]).frame_write_buffer + 1, p_ble_ecs_init->p_init_vals->rw_adv_slot.p_data, copy_length);
                }

                m_slots[i].frame_write_length = p_ble_ecs_init->p_init_vals->rw_adv_slot.char_length;
                if (eddystone_adv_slot_is_configured(i))
                {
                    err_code = eddystone_adv_slot_adv_frame_set(i);
                    APP_ERROR_CHECK(err_code);
                }
            }
            else
            {
                m_slots[i].slot_no = i;
                m_slots[i].adv_intrvl = m_slots[0].adv_intrvl;
                m_slots[i].radio_tx_pwr = m_slots[0].radio_tx_pwr;
                memset((m_slots[i]).frame_write_buffer, 0, 1);
                m_slots[i].frame_write_length = 0;
                memset(&(m_slots[i].adv_frame), 0, sizeof(eddystone_adv_frame_t));
            }
        }
    }
    //Stored configs are available, use these instead
    else
    {
        for (uint8_t i = 0; i < APP_MAX_ADV_SLOTS; i++)
        {
            if (!flash_flags.slot_is_empty[i])
            {
                eddystone_adv_slot_load_from_flash(i);
            }
            else
            {
                if(flash_flags.slot_is_empty[i] != 1 && flash_flags.slot_is_empty[i] != 0xFF)
                {
                    APP_ERROR_CHECK(NRF_ERROR_INVALID_PARAM);
                    //sanity check
                }
                m_slots[i].slot_no = i;
                m_slots[i].adv_intrvl = m_slots[0].adv_intrvl;
                m_slots[i].radio_tx_pwr = m_slots[0].radio_tx_pwr;
                memset((m_slots[i]).frame_write_buffer, 0, 1);
                m_slots[i].frame_write_length = 0;
                memset(&(m_slots[i].adv_frame), 0, sizeof(eddystone_adv_frame_t));
            }
        }
    }
}

static void eddystone_adv_slot_load_from_flash( uint8_t slot_no )
{
    eddystone_flash_slot_config_t config;
    memset(&config, 0, sizeof(config));
    ret_code_t err_code;

    err_code = eddystone_flash_access_slot_configs( slot_no,
                                                    &config,
                                                    EDDYSTONE_FLASH_ACCESS_READ);
    APP_ERROR_CHECK(err_code);
    FLASH_OP_WAIT();

    if (eddystone_flash_read_is_empty((uint8_t*)(&config), FLASH_BLOCK_SIZE))
    {
        m_slots[slot_no].frame_write_length = 0;
        //eddystone_adv_slot_is_configured() will treat a frame_write_length of 0 as not configured
    }
    else
    {
        m_slots[slot_no].adv_intrvl = config.adv_int;
        m_slots[slot_no].radio_tx_pwr = config.radio_tx_pwr;
        ble_ecs_rw_adv_slot_t slot_input;
        slot_input.frame_type = (eddystone_frame_type_t)config.frame_data[0];
        slot_input.p_data = (int8_t*)(&config.frame_data[1]);
        slot_input.char_length = config.data_length;

        if (slot_input.frame_type != EDDYSTONE_FRAME_TYPE_EID)
        {
            eddystone_adv_slot_rw_adv_data_set(slot_no, &slot_input);
        }
        else if (slot_input.frame_type == EDDYSTONE_FRAME_TYPE_EID)
        {
            eddystone_security_eid_slots_restore(slot_no, (eddystone_eid_config_t*)config.frame_data);
            //Since restoring an EID slot does not go through the @ref eddystone_adv_slot_rw_adv_data_set() interface"
            //The frame_write_length must be set to > 1 so that @ref eddystone_adv_slot_is_configured() will treat it
            //As a configured slot
            m_slots[slot_no].frame_write_length = 2;
        }
    }
}

void eddystone_adv_slot_write_to_flash( uint8_t slot_no )
{
    ret_code_t err_code;
    eddystone_flash_slot_config_t config;
    memset(&config, 0, sizeof(config));

    if (eddystone_adv_slot_is_configured(slot_no))
    {
        config.adv_int = m_slots[slot_no].adv_intrvl;
        config.radio_tx_pwr = m_slots[slot_no].radio_tx_pwr;
        if (m_slots[slot_no].frame_write_buffer[0] != EDDYSTONE_FRAME_TYPE_EID)
        {
            memcpy(config.frame_data, m_slots[slot_no].frame_write_buffer, m_slots[slot_no].frame_write_length);
            config.data_length = m_slots[slot_no].frame_write_length;
        }
        else
        {
            eddystone_eid_config_t eid_config;
            eddystone_security_eid_config_get(slot_no, &eid_config);
            memcpy(config.frame_data, &eid_config, sizeof(eddystone_eid_config_t));
            config.data_length = sizeof(eddystone_eid_config_t);
        }

        err_code = eddystone_flash_access_slot_configs( slot_no,
                                                        &config,
                                                        EDDYSTONE_FLASH_ACCESS_WRITE);
    }
    else
    {
        err_code = eddystone_flash_access_slot_configs( slot_no,
                                                        NULL,
                                                        EDDYSTONE_FLASH_ACCESS_CLEAR);
    }
    APP_ERROR_CHECK(err_code);
}

void eddystone_adv_slot_adv_intrvl_set( uint8_t slot_no, ble_ecs_adv_intrvl_t * p_adv_intrvl, bool global )
{
    //Boundary check: if out of bounds, set input value to boundary value
    SLOT_BOUNDARY_CHECK(slot_no);

    uint16_t temp_var = *p_adv_intrvl;
    temp_var = BYTES_SWAP_16BIT(temp_var); //Make temp_var little endian first

    DEBUG_PRINTF(0,"ADV INTERVAL WRITTEN BY CLIENT: 0x%04x \r\n", temp_var);

    if (temp_var < MIN_NON_CONN_ADV_INTERVAL)
    {
        *p_adv_intrvl = MIN_NON_CONN_ADV_INTERVAL;
        *p_adv_intrvl = BYTES_SWAP_16BIT(*p_adv_intrvl); //make big endian
    }
    else if (temp_var > MAX_ADV_INTERVAL)
    {
        *p_adv_intrvl = MAX_ADV_INTERVAL;
        *p_adv_intrvl = BYTES_SWAP_16BIT(*p_adv_intrvl); //make big endian
    }

    if (!global)
    {
        m_slots[slot_no].adv_intrvl = BYTES_SWAP_16BIT(*p_adv_intrvl); //convert dereferenced value back to small endian
    }
    else
    {
        for (uint8_t i = 0; i < APP_MAX_ADV_SLOTS; i++)
        {
            m_slots[i].adv_intrvl = BYTES_SWAP_16BIT(*p_adv_intrvl); //convert dereferenced value back to small endian
        }
    }
}

void eddystone_adv_slot_adv_intrvl_get( uint8_t slot_no, ble_ecs_adv_intrvl_t * p_adv_intrvl )
{
    SLOT_BOUNDARY_CHECK(slot_no);
    if (p_adv_intrvl != NULL)
    {
        memcpy(p_adv_intrvl, &(m_slots[slot_no].adv_intrvl), sizeof(ble_ecs_adv_intrvl_t));
        *p_adv_intrvl = BYTES_SWAP_16BIT(*p_adv_intrvl); //convert dereferenced value to big endian
    }
}

/**@brief Function for setting the ranging data field to be broadcast in the frame
* @param[in]       slot_no         the slot index
* @param[in]       tx_power        the radio tx power to be calibrated to ranging data
*/
static void eddystone_set_ranging_data( uint8_t slot_no, ble_ecs_radio_tx_pwr_t tx_power )
{
    int8_t ranging_data_array[ECS_NUM_OF_SUPORTED_TX_POWER] = ECS_CALIBRATED_RANGING_DATA;
    ble_ecs_radio_tx_pwr_t supported_tx[ECS_NUM_OF_SUPORTED_TX_POWER] = ECS_SUPPORTED_TX_POWER;

    int8_t ranging_data = 0;

    for (uint8_t i = 0; i < ECS_NUM_OF_SUPORTED_TX_POWER; i++)
    {
        if (tx_power == supported_tx[i])
        {
            ranging_data = ranging_data_array[i];
        }
    }

    eddystone_frame_type_t frame_type = (eddystone_frame_type_t)m_slots[slot_no].frame_write_buffer[0];
    if (frame_type == EDDYSTONE_FRAME_TYPE_EID)
    {
        m_slots[slot_no].adv_frame.eid.ranging_data = ranging_data;
    }
    else if (frame_type == EDDYSTONE_FRAME_TYPE_UID)
    {
        m_slots[slot_no].adv_frame.uid.ranging_data = ranging_data;

    }
    else if (frame_type == EDDYSTONE_FRAME_TYPE_URL)
    {
        m_slots[slot_no].adv_frame.url.ranging_data = ranging_data;
    }
}

void eddystone_adv_slot_radio_tx_pwr_set( uint8_t slot_no, ble_ecs_radio_tx_pwr_t * p_radio_tx_pwr, bool global )
{
    SLOT_BOUNDARY_CHECK(slot_no);

    bool match_flag = false; // check if input param matches the list of supported TXs
    ble_ecs_radio_tx_pwr_t supported_tx[ECS_NUM_OF_SUPORTED_TX_POWER] = ECS_SUPPORTED_TX_POWER;
    for (uint8_t i = 0; i < ECS_NUM_OF_SUPORTED_TX_POWER; i++)
    {
        if (*p_radio_tx_pwr == supported_tx[i])
        {
            match_flag = true;
        }
    }

    if (match_flag == true)
    {

        if (global)
        {
            for (uint8_t i = 0; i < APP_MAX_ADV_SLOTS; i++)
            {
                m_slots[i].radio_tx_pwr = *p_radio_tx_pwr;
                eddystone_set_ranging_data(i, m_slots[i].radio_tx_pwr);
            }
        }
        else if (!global)
        {
            m_slots[slot_no].radio_tx_pwr = *p_radio_tx_pwr;
            eddystone_set_ranging_data(slot_no, m_slots[slot_no].radio_tx_pwr);
        }
    }

    else
    {
        *p_radio_tx_pwr = m_slots[slot_no].radio_tx_pwr;
        eddystone_set_ranging_data(slot_no, m_slots[slot_no].radio_tx_pwr);
    }
}

void eddystone_adv_slot_radio_tx_pwr_get( uint8_t slot_no, ble_ecs_radio_tx_pwr_t * p_radio_tx_pwr )
{
    SLOT_BOUNDARY_CHECK(slot_no);
    if (p_radio_tx_pwr != NULL)
    {
        memcpy(p_radio_tx_pwr, &(m_slots[slot_no].radio_tx_pwr), sizeof(ble_ecs_radio_tx_pwr_t));
    }
}

void eddystone_adv_slot_rw_adv_data_set( uint8_t slot_no, ble_ecs_rw_adv_slot_t * p_frame_data )
{
    ret_code_t err_code;
    SLOT_BOUNDARY_CHECK(slot_no);
    if (p_frame_data != NULL)
    {
        uint8_t copy_offset = 1;
        m_slots[slot_no].frame_write_buffer[0] = p_frame_data->frame_type;

        //length > 1 means the client is NOT trying to clear a slot, or not setting an TLM
        if (p_frame_data->char_length > 1)
        {
            memcpy(m_slots[slot_no].frame_write_buffer + copy_offset, p_frame_data->p_data, (p_frame_data->char_length) - copy_offset);
        }

        m_slots[slot_no].frame_write_length = p_frame_data->char_length;

        if (m_slots[slot_no].frame_write_buffer[0] != EDDYSTONE_FRAME_TYPE_EID)
        {
            eddystone_security_eid_slot_destroy(slot_no);
        }

        if (eddystone_adv_slot_is_configured(slot_no))
        {
            err_code = app_sched_event_put(&slot_no, sizeof(slot_no), eddystone_adv_frame_set_scheduler_evt);
            APP_ERROR_CHECK(err_code);
        }
    }
}

void eddystone_adv_slot_rw_adv_data_get( uint8_t slot_no, ble_ecs_rw_adv_slot_t * p_frame_data )
{
    SLOT_BOUNDARY_CHECK(slot_no);
    if (p_frame_data != NULL)
    {
        eddystone_frame_type_t frame_type = (eddystone_frame_type_t)m_slots[slot_no].frame_write_buffer[0];
        p_frame_data->frame_type = frame_type;

        //Used in EID switch case
        uint8_t k_scaler;
        uint32_t clock_val;
        uint8_t eid_read[ECS_EID_READ_LENGTH - 1] = {0};  //subtract frametype

        //If the slot is not configured then it should return emtpy byte to the client
        if (!eddystone_adv_slot_is_configured(slot_no))
        {
            p_frame_data->p_data = NULL;
            p_frame_data->char_length = 0;
        }
        else
        {
            switch (frame_type)
            {
                case EDDYSTONE_FRAME_TYPE_UID:
                    p_frame_data->p_data = (int8_t*)&(m_slots[slot_no].adv_frame.uid.ranging_data);
                    p_frame_data->char_length = EDDYSTONE_UID_LENGTH;
                    break;
                case EDDYSTONE_FRAME_TYPE_URL:
                    p_frame_data->p_data = (int8_t*)&(m_slots[slot_no].adv_frame.url.ranging_data);
                    p_frame_data->char_length = m_slots[slot_no].frame_write_length + 1; //+1 for RSSI byte onto the write length
                    break;
                case EDDYSTONE_FRAME_TYPE_TLM:
                    //The data in adv_frame.tlm is set via a pointer memcpy everytime the advertising_manager
                    //advertises a TLM/eTLM so that upon a read of the slot the user will receive the last
                    //advertised packet, as required by the eddystone spec.
                    if (eddystone_adv_slot_num_of_current_eids(NULL, NULL) == 0)
                    {
                        p_frame_data->p_data = (int8_t*)&(m_slots[slot_no].adv_frame.tlm.version);
                        p_frame_data->char_length = EDDYSTONE_TLM_LENGTH;
                    }
                    else
                    {
                        p_frame_data->p_data = (int8_t*)&(m_slots[slot_no].adv_frame.etlm.version);
                        p_frame_data->char_length = EDDYSTONE_ETLM_LENGTH;
                    }
                    break;
                case EDDYSTONE_FRAME_TYPE_EID:
                    k_scaler = eddystone_security_scaler_get(slot_no);
                    clock_val = eddystone_security_clock_get(slot_no);
                    clock_val = BYTES_REVERSE_32BIT(clock_val); //to big endian
                    memcpy(eid_read, &k_scaler, 1);
                    memcpy(eid_read + 1, (uint8_t*)&clock_val, 4);
                    eddystone_security_eid_get(slot_no,eid_read+5);
                    p_frame_data->p_data = (int8_t*)eid_read;
                    p_frame_data->char_length = ECS_EID_READ_LENGTH;
                    break;
                default:
                    break;
            }
        }
    }
}

ret_code_t eddystone_adv_slot_encrypted_eid_id_key_get( uint8_t slot_no, ble_ecs_eid_id_key_t * p_eid_id_key )
{
    SLOT_BOUNDARY_CHECK(slot_no);
    if (p_eid_id_key != NULL)
    {
        if(m_slots[slot_no].frame_write_buffer[0] == EDDYSTONE_EID_FRAME_TYPE)
        {
            memcpy(p_eid_id_key, &(m_slots[slot_no].encrypted_eid_id_key), sizeof(ble_ecs_eid_id_key_t));
            return NRF_SUCCESS;
        }
        else
        {
            return NRF_ERROR_INVALID_STATE;
        }
    }
    return NRF_ERROR_INVALID_PARAM;
}

void eddystone_adv_slot_encrypted_eid_id_key_set( uint8_t slot_no, ble_ecs_eid_id_key_t * p_eid_id_key )
{
    SLOT_BOUNDARY_CHECK(slot_no);
    if (p_eid_id_key != NULL)
    {
        memcpy(&(m_slots[slot_no].encrypted_eid_id_key),p_eid_id_key, sizeof(ble_ecs_eid_id_key_t));
    }
}

void eddystone_adv_slot_eid_ready( uint8_t slot_no )
{
    m_slots[slot_no].frame_write_buffer[0] = EDDYSTONE_FRAME_TYPE_EID;
    m_slots[slot_no].adv_frame.eid.frame_type = EDDYSTONE_FRAME_TYPE_EID;
    m_slots[slot_no].frame_write_length = ECS_EID_WRITE_ECDH_LENGTH;
    eddystone_set_ranging_data(slot_no, m_slots[slot_no].radio_tx_pwr);
    eddystone_security_eid_get(slot_no, (uint8_t*)m_slots[slot_no].adv_frame.eid.eid);
}

uint8_t eddystone_adv_slot_num_of_configured_slots(uint8_t * p_which_slots_are_configured)
{
    uint8_t slots_count = 0;
    memset(p_which_slots_are_configured, 0xFF, APP_MAX_ADV_SLOTS);
    for (uint8_t i = 0; i < APP_MAX_ADV_SLOTS; i++)
    {
        if (eddystone_adv_slot_is_configured(i))
        {
            *(p_which_slots_are_configured+slots_count) = i;
            slots_count++;
        }
    }
    return slots_count;
}
uint8_t eddystone_adv_slot_num_of_current_eids(uint8_t * p_which_slots_are_eids, bool * p_etlm_required)
{
    uint8_t eid_count = 0;
    bool tlm_exists = false;

    if (p_which_slots_are_eids != NULL)
    {
        memset(p_which_slots_are_eids, 0xFF, APP_MAX_EID_SLOTS);
    }
    for (uint8_t i = 0; i < APP_MAX_EID_SLOTS; i++)
    {
        if (m_slots[i].frame_write_buffer[0] == EDDYSTONE_FRAME_TYPE_TLM)
        {
            tlm_exists = true;
        }

        if (m_slots[i].frame_write_buffer[0] == EDDYSTONE_FRAME_TYPE_EID)
        {
            if (p_which_slots_are_eids != NULL)
            {
                *(p_which_slots_are_eids+eid_count) = i;
            }
            eid_count++;
        }
    }

    if (p_etlm_required != NULL && eid_count > 0 && tlm_exists == true )
    {
        *p_etlm_required = true;
    }
    else if (p_etlm_required != NULL)
    {
        *p_etlm_required = false;
    }

    return eid_count;
}

/**@brief scheduler event to execute in main context (key exchange for EIDs is quite comp. intensive)*/
static void eddystone_adv_frame_set_scheduler_evt(void * p_event_data, uint16_t event_size)
{
    uint8_t slot_no = *(uint8_t*)(p_event_data);
    ret_code_t err_code;
    err_code = eddystone_adv_slot_adv_frame_set(slot_no);
    //If the user wrote something invalid, then change the rw buffer length to 0 so
    //when the user reads it back, they'll know the slot was not succesfully configured
    if (err_code == NRF_ERROR_INVALID_PARAM)
    {
        m_slots[slot_no].frame_write_length = 0;
    }
    else
    {
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for setting advertising frame of the slot with the necessary data in the slot
* @details this function must be called in order for the advertising_manager to broadcast this slot
* @param[in]       slot_no         the slot index
*/
static uint32_t eddystone_adv_slot_adv_frame_set(uint8_t slot_no)
{
    SLOT_BOUNDARY_CHECK(slot_no);
    eddystone_frame_type_t frame_type = (eddystone_frame_type_t)m_slots[slot_no].frame_write_buffer[0];
    uint8_t                eid_slot_positions[APP_MAX_EID_SLOTS];
    switch (frame_type)
    {
        case EDDYSTONE_FRAME_TYPE_UID:
            if (m_slots[slot_no].frame_write_length == ECS_UID_WRITE_LENGTH) //17 bytes
            {
                m_slots[slot_no].adv_frame.uid.frame_type = frame_type;
                eddystone_set_ranging_data(slot_no, m_slots[slot_no].radio_tx_pwr);
                memcpy(m_slots[slot_no].adv_frame.uid.namespace, &(m_slots[slot_no].frame_write_buffer[1]), ECS_UID_WRITE_LENGTH);
                uint8_t rfu[EDDYSTONE_UID_RFU_LENGTH] = {EDDYSTONE_UID_RFU};
                memcpy(m_slots[slot_no].adv_frame.uid.rfu, rfu, EDDYSTONE_UID_RFU_LENGTH);
            }
            else
            {
                return NRF_ERROR_INVALID_PARAM;
            }
            break;
        case EDDYSTONE_FRAME_TYPE_URL:
            if (m_slots[slot_no].frame_write_length <= ECS_URL_WRITE_LENGTH) //up to 19 bytes
            {
                m_slots[slot_no].adv_frame.url.frame_type = frame_type;
                eddystone_set_ranging_data(slot_no, m_slots[slot_no].radio_tx_pwr);
                memcpy(&m_slots[slot_no].adv_frame.url.url_scheme, &(m_slots[slot_no].frame_write_buffer[1]), ECS_URL_WRITE_LENGTH - 1);
            }
            else
            {
                return NRF_ERROR_INVALID_PARAM;
            }
            break;
        case EDDYSTONE_FRAME_TYPE_TLM:
            if ((m_slots[slot_no].frame_write_length == ECS_TLM_WRITE_LENGTH)) //1 byte
            {
                if (eddystone_adv_slot_num_of_current_eids(eid_slot_positions, NULL) == 0)
                {
                    eddystone_tlm_manager_tlm_get(&m_slots[slot_no].adv_frame.tlm);
                }
                else
                {
                    eddystone_tlm_manager_etlm_get(eid_slot_positions[0], &m_slots[slot_no].adv_frame.etlm);
                }
            }
            else
            {
                return NRF_ERROR_INVALID_PARAM;
            }
            break;
        case EDDYSTONE_FRAME_TYPE_EID:

            eddystone_set_ranging_data(slot_no, m_slots[slot_no].radio_tx_pwr);

            if (m_slots[slot_no].frame_write_length == ECS_EID_WRITE_ECDH_LENGTH) //34 bytes
            {
                ret_code_t err_code;
                m_slots[slot_no].adv_frame.eid.frame_type = frame_type;

                uint8_t public_edch[ECS_ECDH_KEY_SIZE];
                uint8_t scaler_k = m_slots[slot_no].frame_write_buffer[ECS_EID_WRITE_ECDH_LENGTH-1]; // last byte
                memcpy(public_edch, &(m_slots[slot_no].frame_write_buffer[1]), ECS_EID_WRITE_ECDH_LENGTH - 2); //no frametype and no exponent

                err_code = eddystone_security_client_pub_ecdh_receive(slot_no, public_edch, scaler_k );
                RETURN_IF_ERROR(err_code);

                //note: the adv frame is completely set when the security module calls back to the ble_handler
                //when EIDs have been generated, eddystone_adv_slot_eid_ready is called
                //and the EID is fetched from the security module and placed into the adv_frame
            }
            else if (m_slots[slot_no].frame_write_length == ECS_EID_WRITE_IDK_LENGTH) // 18 bytes
            {
                ret_code_t err_code;

                m_slots[slot_no].adv_frame.eid.frame_type = frame_type;
                uint8_t encrypted_key[ECS_AES_KEY_SIZE];
                uint8_t scaler_k = m_slots[slot_no].frame_write_buffer[ECS_EID_WRITE_IDK_LENGTH-1]; // last byte
                memcpy(encrypted_key, &(m_slots[slot_no].frame_write_buffer[1]), ECS_EID_WRITE_IDK_LENGTH - 2); //no frametype and no exponent

                err_code = eddystone_security_shared_ik_receive(slot_no, encrypted_key, scaler_k);
                RETURN_IF_ERROR(err_code);

                //note: the adv frame is completely set when the security module calls back to the ble_handler
                //when EIDs have been generated, eddystone_adv_slot_eid_ready is called
                //and the EID is fetched from the security module and placed into the adv_frame
            }
            else
            {
                return NRF_ERROR_INVALID_PARAM;
            }
            break;
        default:
            break;
    }
    return NRF_SUCCESS;
}

bool eddystone_adv_slot_is_configured (uint8_t slot_no)
{
    SLOT_BOUNDARY_CHECK(slot_no);
    //Client wrote an empty array
    if ( m_slots[slot_no].frame_write_length == 0 )
    {
        return false;
    }
    //Client wrote a single byte of 0
    else if ( m_slots[slot_no].frame_write_length == 1 && m_slots[slot_no].frame_write_buffer[0] == 0 )
    {
        return false;
    }
    else
    {
        return true;
    }
}

void eddystone_adv_slot_params_get( uint8_t slot_no, eddystone_adv_slot_params_t * p_params)
{
    p_params->adv_intrvl            = m_slots[slot_no].adv_intrvl;
    p_params->radio_tx_pwr          = m_slots[slot_no].radio_tx_pwr;
    p_params->frame_type            = (eddystone_frame_type_t)m_slots[slot_no].frame_write_buffer[0];
    p_params->p_adv_frame           = &(m_slots[slot_no].adv_frame);
    p_params->url_frame_length      = m_slots[slot_no].frame_write_length+1; // Add the RSSI byte length
}
