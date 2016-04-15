#include "eddystone_adv_slot.h"
#include "eddystone_app_config.h"
#include "nrf_error.h"
#include "ble_gap.h"
#include "endian_convert.h"
#include "eddystone_security.h"
#include "macros_common.h"
#include "eddystone_tlm_manager.h"
#include "debug_config.h"
#include "app_scheduler.h"
#include <stdint.h>
#include <string.h>

#ifdef SLOT_DEBUG
    #include "SEGGER_RTT.h"
    #define DEBUG_PRINTF SEGGER_RTT_printf
#else
    #define DEBUG_PRINTF(...)
#endif

//Forward Declaration:
uint32_t eddystone_adv_slot_adv_frame_set(uint8_t slot_no);
static void eddystone_adv_frame_set_scheduler_evt( void * p_event_data, uint16_t event_size );

static eddystone_adv_slot_t m_slots[APP_MAX_ADV_SLOTS];

// #define SLOT_TEST

void eddystone_adv_slots_init( ble_ecs_init_t * p_ble_ecs_init )
{
    //Copy length corresponds to the length of JUST the data in the frame, excluding frame type
    uint16_t copy_length = p_ble_ecs_init->p_init_vals->rw_adv_slot.char_length - 1;
    ret_code_t err_code;
    for (uint8_t i = 0; i < APP_MAX_ADV_SLOTS; i++)
    {
        if (i == 0)
        {
            m_slots[i].slot_no = i;
            m_slots[i].adv_intrvl = p_ble_ecs_init->p_init_vals->adv_intrvl;
            m_slots[i].radio_tx_pwr = p_ble_ecs_init->p_init_vals->radio_tx_pwr;
            m_slots[i].frame_rw_buffer[0] = p_ble_ecs_init->p_init_vals->rw_adv_slot.frame_type;
            memcpy((m_slots[i]).frame_rw_buffer + 1, p_ble_ecs_init->p_init_vals->rw_adv_slot.p_data, copy_length);
            m_slots[i].frame_rw_length = p_ble_ecs_init->p_init_vals->rw_adv_slot.char_length;
            memcpy(&(m_slots[i].eid_id_key.key), (p_ble_ecs_init->p_init_vals->eid_id_key.key), ECS_AES_KEY_SIZE);
            m_slots[i].is_configured = true;
            err_code = eddystone_adv_slot_adv_frame_set(i);
            APP_ERROR_CHECK(err_code);
        }
        #ifdef SLOT_TEST
            else if (i == 1)
            {
                m_slots[i].slot_no = i;
                m_slots[i].adv_intrvl = p_ble_ecs_init->p_init_vals->adv_intrvl;
                m_slots[i].radio_tx_pwr = p_ble_ecs_init->p_init_vals->radio_tx_pwr;
                m_slots[i].frame_rw_buffer[0] = 0x00;
                memcpy((m_slots[i]).frame_rw_buffer + 1, p_ble_ecs_init->p_init_vals->rw_adv_slot.p_data, copy_length);
                m_slots[i].frame_rw_length = copy_length + 1; // +1 for frame type
                memcpy(&(m_slots[i].eid_id_key.key), (p_ble_ecs_init->p_init_vals->eid_id_key.key), ECS_AES_KEY_SIZE);
                m_slots[i].adv_frame.uid.frame_type = 0x00;
                m_slots[i].adv_frame.uid.ranging_data = 0x00;

                int8_t namesp[EDDYSTONE_UID_NAMESPACE_LENGTH] = {0x01};
                int8_t inst[EDDYSTONE_UID_INSTANCE_LENGTH] = {0x02};
                int8_t rfu[EDDYSTONE_UID_RFU_LENGTH] = {0x00};
                memcpy(m_slots[i].adv_frame.uid.namespace, namesp, EDDYSTONE_UID_NAMESPACE_LENGTH);
                memcpy(m_slots[i].adv_frame.uid.instance, inst, EDDYSTONE_UID_INSTANCE_LENGTH);
                memcpy(m_slots[i].adv_frame.uid.rfu, rfu, EDDYSTONE_UID_RFU_LENGTH);
                m_slots[i].is_configured = true;
            }
            else if (i == 2)
            {
                m_slots[i].slot_no = i;
                m_slots[i].adv_intrvl = p_ble_ecs_init->p_init_vals->adv_intrvl;
                m_slots[i].radio_tx_pwr = p_ble_ecs_init->p_init_vals->radio_tx_pwr;
                m_slots[i].frame_rw_buffer[0] = 0x00;
                memcpy((m_slots[i]).frame_rw_buffer + 1, p_ble_ecs_init->p_init_vals->rw_adv_slot.p_data, copy_length);
                m_slots[i].frame_rw_length = copy_length + 1; // +1 for frame type
                memcpy(&(m_slots[i].eid_id_key.key), (p_ble_ecs_init->p_init_vals->eid_id_key.key), ECS_AES_KEY_SIZE);
                m_slots[i].adv_frame.uid.frame_type = 0x00;
                m_slots[i].adv_frame.uid.ranging_data = 0x00;

                int8_t namesp[EDDYSTONE_UID_NAMESPACE_LENGTH] = {0x05};
                int8_t inst[EDDYSTONE_UID_INSTANCE_LENGTH] = {0x05};
                int8_t rfu[EDDYSTONE_UID_RFU_LENGTH] = {0x00};
                memcpy(m_slots[i].adv_frame.uid.namespace, namesp, EDDYSTONE_UID_NAMESPACE_LENGTH);
                memcpy(m_slots[i].adv_frame.uid.instance, inst, EDDYSTONE_UID_INSTANCE_LENGTH);
                memcpy(m_slots[i].adv_frame.uid.rfu, rfu, EDDYSTONE_UID_RFU_LENGTH);
                m_slots[i].is_configured = true;
            }
        #endif
        else
        {
            m_slots[i].slot_no = i;
            m_slots[i].adv_intrvl = m_slots[0].adv_intrvl;
            m_slots[i].radio_tx_pwr = m_slots[0].radio_tx_pwr;
            memset((m_slots[i]).frame_rw_buffer, 0, 1);
            m_slots[i].frame_rw_length = 0;
            memset(&(m_slots[i].adv_frame), 0, ECS_AES_KEY_SIZE);
            m_slots[i].is_configured = false;
        }
    }
}

void eddystone_adv_slot_adv_intrvl_set( uint8_t slot_no, ble_ecs_adv_intrvl_t * p_adv_intrvl, bool global )
{
    //Boundary check: if out of bounds, set input value to boundary value
    SLOT_BOUNDARY_CHECK(slot_no);
    
    //Make temp_var little endian first
    uint16_t temp_var = *p_adv_intrvl;
    temp_var = BYTES_SWAP_16BIT(temp_var);

    DEBUG_PRINTF(0,"ADV INTERVAL WRITTEN BY CLIENT: 0x%04x \r\n", temp_var);

    if (temp_var < MIN_NON_CONN_ADV_INTERVAL) //convert constants to big endian values
    {
        *p_adv_intrvl = MIN_NON_CONN_ADV_INTERVAL;
        *p_adv_intrvl = BYTES_SWAP_16BIT(*p_adv_intrvl);
    }
    else if (temp_var > MAX_ADV_INTERVAL)
    {
        *p_adv_intrvl = MAX_ADV_INTERVAL;
        *p_adv_intrvl = BYTES_SWAP_16BIT(*p_adv_intrvl);
    }

    if (!global)
    {
        m_slots[slot_no].adv_intrvl = BYTES_SWAP_16BIT(*p_adv_intrvl); //convert dereferenced value to small endian
    }
    else
    {
        for (uint8_t i = 0; i < APP_MAX_ADV_SLOTS; i++)
        {
            m_slots[i].adv_intrvl = BYTES_SWAP_16BIT(*p_adv_intrvl); //convert dereferenced value to small endian
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

    eddystone_frame_type_t frame_type = (eddystone_frame_type_t)m_slots[slot_no].frame_rw_buffer[0];
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
    bool match_flag = false;
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

void eddystone_adv_slot_rw_buffer_data_set( uint8_t slot_no, ble_ecs_rw_adv_slot_t * p_frame_data )
{
    ret_code_t err_code;
    SLOT_BOUNDARY_CHECK(slot_no);
    if (p_frame_data != NULL)
    {
        uint8_t copy_offset = 1;
        m_slots[slot_no].frame_rw_buffer[0] = p_frame_data->frame_type;

        //length > 1 means the client is NOT trying to clear a slot, or setting an TLM
        if (p_frame_data->char_length > 1)
        {
            memcpy(m_slots[slot_no].frame_rw_buffer + copy_offset, p_frame_data->p_data, (p_frame_data->char_length) - copy_offset);
        }

        m_slots[slot_no].frame_rw_length = p_frame_data->char_length;

        if (m_slots[slot_no].frame_rw_buffer[0] != EDDYSTONE_FRAME_TYPE_EID)
        {
            eddystone_security_eid_state_destroy(slot_no);
        }

        if (eddystone_adv_slot_is_configured(slot_no))
        {
            err_code = app_sched_event_put(&slot_no, sizeof(slot_no), eddystone_adv_frame_set_scheduler_evt);
            APP_ERROR_CHECK(err_code);
        }
    }
}

void eddystone_adv_slot_rw_buffer_data_get( uint8_t slot_no, ble_ecs_rw_adv_slot_t * p_frame_data )
{
    SLOT_BOUNDARY_CHECK(slot_no);
    if (p_frame_data != NULL)
    {
        eddystone_frame_type_t frame_type = (eddystone_frame_type_t)m_slots[slot_no].frame_rw_buffer[0];
        p_frame_data->frame_type = frame_type;

        DEBUG_PRINTF(0,"Reading Frame type: 0x%02x \r\n", frame_type);

        //Used in EID switch case
        uint8_t k_scaler;
        uint32_t clock_val;
        uint8_t eid_read[ECS_EID_READ_LENGTH - 1] = {0};  //subtract frametype

        //If the slot is not configured then it should return emtpy data to the client
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
                    p_frame_data->char_length = m_slots[slot_no].frame_rw_length + 1; //+1 for RSSI byte onto the write length
                    break;
                case EDDYSTONE_FRAME_TYPE_TLM:
                    if (eddystone_adv_slot_num_of_current_eids(NULL) == 0)
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

ret_code_t eddystone_adv_slot_eid_id_key_get( uint8_t slot_no, ble_ecs_eid_id_key_t * p_eid_id_key )
{
    SLOT_BOUNDARY_CHECK(slot_no);
    if (p_eid_id_key != NULL)
    {
        if(m_slots[slot_no].frame_rw_buffer[0] == EDDYSTONE_EID_FRAME_TYPE)
        {
            memcpy(p_eid_id_key, &(m_slots[slot_no].eid_id_key), sizeof(ble_ecs_eid_id_key_t));
            return NRF_SUCCESS;
        }
        else
        {
            return NRF_ERROR_INVALID_STATE;
        }
    }
    return NRF_ERROR_INVALID_PARAM;
}

void eddystone_adv_slot_eid_id_key_set( uint8_t slot_no, ble_ecs_eid_id_key_t * p_eid_id_key )
{
    SLOT_BOUNDARY_CHECK(slot_no);
    if (p_eid_id_key != NULL)
    {
        memcpy(&(m_slots[slot_no].eid_id_key),p_eid_id_key, sizeof(ble_ecs_eid_id_key_t));
    }
}

void eddystone_adv_slot_eid_set( uint8_t slot_no )
{
    m_slots[slot_no].is_configured = true;
    m_slots[slot_no].frame_rw_buffer[0] = EDDYSTONE_FRAME_TYPE_EID;
    m_slots[slot_no].adv_frame.eid.frame_type = EDDYSTONE_FRAME_TYPE_EID;
    eddystone_set_ranging_data(slot_no, m_slots[slot_no].radio_tx_pwr);
    eddystone_security_eid_get(slot_no, (uint8_t*)m_slots[slot_no].adv_frame.eid.eid);
}

uint8_t eddystone_adv_slot_num_of_configured_slots(uint8_t * p_which_slots_are_configured)
{
    uint8_t slots_count = 0;
    memset(p_which_slots_are_configured, 0xFF, APP_MAX_ADV_SLOTS);
    for (uint8_t i = 0; i < APP_MAX_ADV_SLOTS; i++)
    {
        if (m_slots[i].is_configured)
        {
            *(p_which_slots_are_configured+slots_count) = i;
            slots_count++;
        }
    }
    return slots_count;
}
uint8_t eddystone_adv_slot_num_of_current_eids(uint8_t * p_which_slots_are_eids)
{
    uint8_t eid_count = 0;
    if (p_which_slots_are_eids != NULL)
    {
        memset(p_which_slots_are_eids, 0xFF, APP_MAX_EID_SLOTS);
    }
    for (uint8_t i = 0; i < APP_MAX_EID_SLOTS; i++)
    {
        if (m_slots[i].frame_rw_buffer[0] == EDDYSTONE_FRAME_TYPE_EID)
        {
            if (p_which_slots_are_eids != NULL)
            {
                *(p_which_slots_are_eids+eid_count) = i;
            }
            eid_count++;
        }
    }
    return eid_count;
}

/**@brief scheduler event to execute in main context (key exchange for EIDs is quite intensive)*/
static void eddystone_adv_frame_set_scheduler_evt(void * p_event_data, uint16_t event_size)
{
    uint8_t slot_no = *(uint8_t*)(p_event_data);
    ret_code_t err_code;
    err_code = eddystone_adv_slot_adv_frame_set(slot_no);
    //If the user wrote something invalid, then change the rw buffer length to 0 so
    //when the user reads it back, they'll know the slot was not succesfully configured
    if (err_code == NRF_ERROR_INVALID_PARAM)
    {
        m_slots[slot_no].frame_rw_length = 0;
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
    eddystone_frame_type_t frame_type = (eddystone_frame_type_t)m_slots[slot_no].frame_rw_buffer[0];
    uint8_t                eid_slot_positions[APP_MAX_EID_SLOTS];
    switch (frame_type)
    {
        case EDDYSTONE_FRAME_TYPE_UID:
            if (m_slots[slot_no].frame_rw_length == ECS_UID_WRITE_LENGTH) //17 bytes
            {
                m_slots[slot_no].adv_frame.uid.frame_type = frame_type;
                eddystone_set_ranging_data(slot_no, m_slots[slot_no].radio_tx_pwr);
                memcpy(m_slots[slot_no].adv_frame.uid.namespace, &(m_slots[slot_no].frame_rw_buffer[1]), ECS_UID_WRITE_LENGTH);
                uint8_t rfu[EDDYSTONE_UID_RFU_LENGTH] = {EDDYSTONE_UID_RFU};
                memcpy(m_slots[slot_no].adv_frame.uid.rfu, rfu, EDDYSTONE_UID_RFU_LENGTH);
            }
            else
            {
                return NRF_ERROR_INVALID_PARAM;
            }
            break;
        case EDDYSTONE_FRAME_TYPE_URL:
            if (m_slots[slot_no].frame_rw_length <= ECS_URL_WRITE_LENGTH) //up to 19 bytes
            {
                m_slots[slot_no].adv_frame.url.frame_type = frame_type;
                eddystone_set_ranging_data(slot_no, m_slots[slot_no].radio_tx_pwr);
                memcpy(&m_slots[slot_no].adv_frame.url.url_scheme, &(m_slots[slot_no].frame_rw_buffer[1]), ECS_URL_WRITE_LENGTH - 1);
            }
            else
            {
                return NRF_ERROR_INVALID_PARAM;
            }
            break;
        case EDDYSTONE_FRAME_TYPE_TLM:
            if ((m_slots[slot_no].frame_rw_length == ECS_TLM_WRITE_LENGTH)) //1 byte
            {
                if (eddystone_adv_slot_num_of_current_eids(eid_slot_positions) == 0)
                {
                    DEBUG_PRINTF(0,"setting TLM \r\n",0);
                    eddystone_tlm_manager_tlm_get(&m_slots[slot_no].adv_frame.tlm);
                }
                else
                {
                    DEBUG_PRINTF(0,"setting eTLM \r\n",0);
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

            if (m_slots[slot_no].frame_rw_length == ECS_EID_WRITE_ECDH_LENGTH) //34 bytes
            {
                ret_code_t err_code;
                m_slots[slot_no].adv_frame.eid.frame_type = frame_type;

                uint8_t public_edch[ECS_ECDH_KEY_SIZE];
                uint8_t scaler_k = m_slots[slot_no].frame_rw_buffer[ECS_EID_WRITE_ECDH_LENGTH-1]; // last byte
                memcpy(public_edch, &(m_slots[slot_no].frame_rw_buffer[1]), ECS_EID_WRITE_ECDH_LENGTH - 2); //no frametype and no exponent

                err_code = eddystone_security_client_pub_ecdh_receive(slot_no, public_edch, scaler_k );
                RETURN_IF_ERROR(err_code);

                //note: the adv frame is completely set when the security module calls back to the ble_handler
                //when EIDs have been generated, eddystone_adv_slot_eid_set is called
                //and the EID is fetched from the security module and placed into the adv_frame
            }
            else if (m_slots[slot_no].frame_rw_length == ECS_EID_WRITE_IDK_LENGTH) // 18 bytes
            {
                ret_code_t err_code;

                m_slots[slot_no].adv_frame.eid.frame_type = frame_type;
                uint8_t encrypted_key[ECS_AES_KEY_SIZE];
                uint8_t scaler_k = m_slots[slot_no].frame_rw_buffer[ECS_EID_WRITE_IDK_LENGTH-1]; // last byte
                memcpy(encrypted_key, &(m_slots[slot_no].frame_rw_buffer[1]), ECS_EID_WRITE_IDK_LENGTH - 2); //no frametype and no exponent

                err_code = eddystone_security_shared_ik_receive(slot_no, encrypted_key, scaler_k);
                RETURN_IF_ERROR(err_code);

                //note: the adv frame is completely set when the security module calls back to the ble_handler
                //when EIDs have been generated, eddystone_adv_slot_eid_set is called
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
    if ( m_slots[slot_no].frame_rw_length == 0 )
    {
        m_slots[slot_no].is_configured = false;
    }
    //Client write a single byte of 0
    else if ( m_slots[slot_no].frame_rw_length == 1 && m_slots[slot_no].frame_rw_buffer[0] == 0 )
    {
        m_slots[slot_no].is_configured = false;
    }
    else
    {
        m_slots[slot_no].is_configured = true;
    }
    return m_slots[slot_no].is_configured;
}

void eddystone_adv_slot_params_get( uint8_t slot_no, eddystone_adv_slot_params_t * p_params)
{
    p_params->adv_intrvl            = m_slots[slot_no].adv_intrvl;
    p_params->radio_tx_pwr          = m_slots[slot_no].radio_tx_pwr;
    p_params->frame_type            = (eddystone_frame_type_t)m_slots[slot_no].frame_rw_buffer[0];
    p_params->p_adv_frame           = &(m_slots[slot_no].adv_frame);
    p_params->url_frame_length      = m_slots[slot_no].frame_rw_length+1; // Add the RSSI byte length
}
