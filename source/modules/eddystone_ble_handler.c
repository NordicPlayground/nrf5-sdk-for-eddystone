//TODO: Add IP blurb here

#include "eddystone_ble_handler.h"
#include "bsp.h"
#include "ble_advdata.h"
#include "ble_gap.h"
#include "nordic_common.h"
#include "softdevice_handler.h"
#include "app_error.h"
#include "ble_ecs.h"
#include "ecs_defs.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "eddystone_app_config.h"
#include "eddystone_adv_slot.h"
#include "eddystone_security.h"
#include "eddystone_registration_ui.h"
#include "pstorage_platform.h"
#include "debug_config.h"

#define RANDOMIZE_MAC

#ifdef GATT_DEBUG
    #include "SEGGER_RTT.h"
    #define DEBUG_PRINTF SEGGER_RTT_printf
#else
    #define DEBUG_PRINTF(...)
#endif

static ble_ecs_t            m_ble_ecs;                                    /**< Struct identifying the Eddystone Config Service. */
static uint16_t             m_conn_handle = BLE_CONN_HANDLE_INVALID;      /**< The current connection handle. */
//Forward Declartions:
static ble_ecs_lock_state_read_t ble_eddystone_is_unlocked(void);
static void ble_eddystone_lock_beacon(void);
static void reset_active_slot(void);

/**@brief Function for the application's SoftDevice event handler.
 *
 * @param[in] p_ble_evt SoftDevice event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t                         err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            DEBUG_PRINTF(0,"Connected! \r\n",0);
            //Reset active slot to 0 on a new connection
            reset_active_slot();
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            DEBUG_PRINTF(0,"Disconnected! \r\n",0);
            switch (ble_eddystone_is_unlocked())
            {
                case BLE_ECS_LOCK_STATE_UNLOCKED:
                    ble_eddystone_lock_beacon();
                    break;
                case BLE_ECS_LOCK_STATE_UNLOCKED_AUTO_RELOCK_DISABLED:
                    //Don't unlock it
                    break;
                default:
                    break;
            }
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}

/**@brief Function for dispatching a SoftDevice event to all modules with a SoftDevice
 *        event handler.
 *
 * @details This function is called from the SoftDevice event interrupt handler after a
 *          SoftDevice event has been received.
 *
 * @param[in] p_ble_evt  SoftDevice event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    ble_conn_params_on_ble_evt(p_ble_evt);
    eddystone_advertising_manager_on_ble_evt(p_ble_evt);
    ble_ecs_on_ble_evt(&m_ble_ecs, p_ble_evt);
    on_ble_evt(p_ble_evt);
    ble_advertising_on_ble_evt(p_ble_evt);
}

/**@brief Function for dispatching system events from the SoftDevice.
 *
 * @details This function is called from the SoftDevice event interrupt handler after a
 *          SoftDevice event has been received.
 *
 * @param[in] evt_id  System event id.
 */
static void sys_evt_dispatch(uint32_t evt_id)
{
    if(evt_id == NRF_EVT_FLASH_OPERATION_SUCCESS ||
       evt_id == NRF_EVT_FLASH_OPERATION_ERROR)
    {
        pstorage_sys_event_handler(evt_id);
    }
}

/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;
    nrf_clock_lf_cfg_t lf_clock_config;
    lf_clock_config.source = NRF_CLOCK_LF_SRC_XTAL;
    lf_clock_config.rc_ctiv = 0;
    lf_clock_config.rc_temp_ctiv = 0;
    lf_clock_config.xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_20_PPM;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(&lf_clock_config, NULL);

    ble_enable_params_t ble_enable_params;
    err_code = softdevice_enable_get_default_config(CENTRAL_LINK_COUNT,
                                                    PERIPHERAL_LINK_COUNT,
                                                    &ble_enable_params);
    APP_ERROR_CHECK(err_code);

    //Check the ram settings against the used number of links
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT,PERIPHERAL_LINK_COUNT);

    // Enable BLE stack.
    err_code = softdevice_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);

    // Subscribe for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);

    //Subscribe for System events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for the GAP initialization.
*
* @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
*          the device. It also sets the permissions and appearance.
*/
static void gap_params_init(void)
{
   uint32_t                err_code;
   ble_gap_conn_params_t   gap_conn_params;
   ble_gap_conn_sec_mode_t sec_mode;

   uint8_t                 device_name[] = APP_DEVICE_NAME;

   BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

   err_code = sd_ble_gap_device_name_set(&sec_mode,
                                         device_name,
                                         strlen((const char *)device_name));
   APP_ERROR_CHECK(err_code);

   memset(&gap_conn_params, 0, sizeof(gap_conn_params));

   gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
   gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
   gap_conn_params.slave_latency     = SLAVE_LATENCY;
   gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

   err_code = sd_ble_gap_ppcp_set(&gap_conn_params);

   APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);

}

static void reset_active_slot(void)
{
    ret_code_t err_code;
    uint8_t reset_slot = 0;
    ble_gatts_value_t value = {.len = 1, .offset = 0, .p_value = &reset_slot};


    err_code = sd_ble_gatts_value_set(m_conn_handle,
                                      m_ble_ecs.active_slot_handles.value_handle,
                                      &value);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function to check if the beacon is unlocked.
 *
 * @details This function will fetch the appropriate fields in the broadcast capabilities
 *          characteristic to check for support
 */
static ble_ecs_lock_state_read_t ble_eddystone_is_unlocked(void)
{
    ret_code_t err_code;
    uint8_t    lock_state;
    ble_gatts_value_t value = {.len = sizeof(lock_state), .offset = 0, .p_value = &lock_state};


    err_code = sd_ble_gatts_value_get(m_conn_handle,
                                      m_ble_ecs.lock_state_handles.value_handle,
                                      &value);
    APP_ERROR_CHECK(err_code);

    return (ble_ecs_lock_state_read_t)lock_state;
}

/**@brief Function to check if variable advertising interval is supported.
 *
 * @details This function will fetch the appropriate fields in the broadcast capabilities
 *          characteristic to check for support
 */
static bool ble_eddystone_is_var_adv_supported(void)
{
    ret_code_t          err_code;
    uint8_t             is_supported;
    const uint8_t       CAP_BITFIELD_POS = 3;
    ble_gatts_value_t   value = {.len = sizeof(is_supported), .offset = CAP_BITFIELD_POS, .p_value = &is_supported};

    err_code = sd_ble_gatts_value_get(m_conn_handle,
                                      m_ble_ecs.brdcst_cap_handles.value_handle,
                                      &value);
    APP_ERROR_CHECK(err_code);

    return ((is_supported & ECS_BRDCST_VAR_ADV_SUPPORTED_Msk) >> ECS_BRDCST_VAR_ADV_SUPPORTED_Pos
            ==
            ECS_BRDCST_VAR_ADV_SUPPORTED_Yes);
}

/**@brief Function to check if variable advertising tx power is supported.
 *
 * @details This function will fetch the appropriate fields in the broadcast capabilities
 *          characteristic to check for support
 */
static bool ble_eddystone_is_var_tx_pwr_supported(void)
{
    ret_code_t          err_code;
    uint8_t             is_supported;
    const uint8_t       CAP_BITFIELD_POS = 3;
    ble_gatts_value_t   value = {.len = sizeof(is_supported), .offset = CAP_BITFIELD_POS, .p_value = &is_supported};

    err_code = sd_ble_gatts_value_get(m_conn_handle,
                                      m_ble_ecs.brdcst_cap_handles.value_handle,
                                      &value);
    APP_ERROR_CHECK(err_code);

    return ((is_supported & ECS_BRDCST_VAR_TX_POWER_SUPPORTED_Msk) >> ECS_BRDCST_VAR_TX_POWER_SUPPORTED_Pos
            ==
            ECS_BRDCST_VAR_TX_POWER_SUPPORTED_Yes);

}

/**@brief Function to get the active slot of the beacon.
 *
 * @details This function will fetch the active slot characteristic value
 *
 */
static uint8_t ble_eddystone_active_slot_get(void)
{
    ret_code_t          err_code;
    uint8_t             active_slot;
    ble_gatts_value_t   value = {.len = sizeof(uint8_t), .offset = 0, .p_value = &active_slot};
    err_code = sd_ble_gatts_value_get(m_conn_handle,
                                      m_ble_ecs.active_slot_handles.value_handle,
                                      &value);
    APP_ERROR_CHECK(err_code);
    //boundary checking
    if (active_slot > APP_MAX_ADV_SLOTS)
    {
        active_slot = APP_MAX_ADV_SLOTS;
    }
    return active_slot;
}

/**@brief Function to lock the beacon (change lock state characteristic to LOCKED)
 */
static void ble_eddystone_lock_beacon(void)
{
    ret_code_t err_code;
    ble_ecs_lock_state_read_t unlock = BLE_ECS_LOCK_STATE_LOCKED;
    ble_gatts_value_t   value = {.len = sizeof(ble_ecs_lock_state_read_t), .offset = 0, .p_value = &unlock};
    err_code = sd_ble_gatts_value_set(m_conn_handle,
                                      m_ble_ecs.lock_state_handles.value_handle,
                                      &value);
   APP_ERROR_CHECK(err_code);
}

/**@brief Callback function to receive messages from the security module
 *
 * @details Need to be passed in during eddystone_security_init()
 * @params[in]  slot_no     Index of the slot
 * @params[in]  msg_type    Message type corersponding to different security items
 */
static void ble_eddystone_security_cb(uint8_t slot_no,
                                      eddystone_security_msg_t msg_type)
{
    ble_ecs_eid_id_key_t encrypted_id_key;
    ble_ecs_public_ecdh_key_t pub_ecdh_key;

    ret_code_t err_code;
    static ble_gatts_value_t   value;
    ble_ecs_lock_state_read_t unlock = BLE_ECS_LOCK_STATE_UNLOCKED;

    ble_gap_addr_t new_address;
    new_address.addr_type = BLE_GAP_ADDR_TYPE_PUBLIC;

    uint8_t  bytes_available;
    const uint8_t ADDR_SIZE = 6;

    switch (msg_type)
    {
        case EDDYSTONE_SECURITY_MSG_UNLOCKED:
            value.len = sizeof(ble_ecs_lock_state_read_t);
            value.offset = 0;
            value.p_value = &unlock;

            err_code = sd_ble_gatts_value_set(m_conn_handle,
                                              m_ble_ecs.lock_state_handles.value_handle,
                                              &value);
            APP_ERROR_CHECK(err_code);
            break;
        case EDDYSTONE_SECURITY_MSG_EID:
            eddystone_adv_slot_eid_set(slot_no);

            #ifdef RANDOMIZE_MAC
            //Randomize the MAC address on every EID generation
            sd_rand_application_bytes_available_get(&bytes_available);
            while (bytes_available < ADDR_SIZE)
            {
               //wait for SD to acquire enough RNs
               sd_rand_application_bytes_available_get(&bytes_available);
            }
            sd_rand_application_vector_get(new_address.addr, ADDR_SIZE);

            err_code = sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &new_address);
            APP_ERROR_CHECK(err_code);
            #endif
            break;

        case EDDYSTONE_SECURITY_MSG_IK:
            DEBUG_PRINTF(0, "Encrypted Identity Key Ready! \r\n", 0);
            eddystone_security_encrypted_eid_id_key_get(slot_no, (uint8_t*)encrypted_id_key.key);
            //Set the EID ID key in the slot so it can be exposed in the characteristic
            eddystone_adv_slot_eid_id_key_set(slot_no, &encrypted_id_key);
            break;

        case EDDYSTONE_SECURITY_MSG_ECDH:
            DEBUG_PRINTF(0, "ECDH Pub Key Ready! \r\n", 0);
            eddystone_security_pub_ecdh_get(slot_no, (uint8_t *)pub_ecdh_key.key);
            value.len = sizeof(ble_ecs_public_ecdh_key_t);
            value.offset = 0,
            value.p_value = (uint8_t *)pub_ecdh_key.key;
            err_code = sd_ble_gatts_value_set(m_conn_handle,
                                              m_ble_ecs.pub_ecdh_key_handles.value_handle,
                                              &value);
            if (err_code != NRF_SUCCESS)
            {
                __NOP();
            }
            APP_ERROR_CHECK(err_code);
            break;
    }
}

/**@brief Function handling all write requests from the Central.
 *
 * @param[in]   p_ecs       Pointer to the eddystone configuration service
 * @param[in]   evt_type    Type of event: corresponding to each characteristic in the service being written to
 * @param[in]   val_handle  Value handle field of the characteristic handle of the characteristic being written to
 * @param[in]   p_data      Pointer to the data to be written
 * @param[in]   length      length of the data to be written
 *
 */
static void ecs_write_evt_handler(ble_ecs_t *        p_ecs,
                                  ble_ecs_evt_type_t evt_type,
                                  uint16_t           val_handle,
                                  uint8_t *          p_data,
                                  uint16_t           length)
{
    ret_code_t                            err_code;
    ble_gatts_rw_authorize_reply_params_t reply;
    memset(&reply, 0, sizeof(reply));
    bool long_write_overwrite_flag = false;

    reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
    reply.params.write.update      = 1;
    reply.params.write.offset      = 0;

    //Allow the write request when unlocked, otherwise deny access
    if (ble_eddystone_is_unlocked() && (evt_type != BLE_ECS_EVT_UNLOCK))
    {
        ble_ecs_rw_adv_slot_t slot_data;
        uint8_t slot_no = ble_eddystone_active_slot_get();

        //Used in lock state case
        uint8_t value_buffer[ECS_ADV_SLOT_CHAR_LENGTH_MAX] = {0};
        ble_gatts_value_t value = {.len = sizeof(value_buffer), .offset = 0, .p_value = &(value_buffer[0])};

        switch (evt_type)
        {
            case BLE_ECS_EVT_ACTIVE_SLOT:
                //boundary checking
                if (*p_data > APP_MAX_ADV_SLOTS - 1)
                {
                    *p_data = APP_MAX_ADV_SLOTS - 1;
                }
                break;

            case BLE_ECS_EVT_ADV_INTRVL:
                DEBUG_PRINTF(0, "Variable Adv Support: %d\r\n", ble_eddystone_is_var_adv_supported());
                if (ble_eddystone_is_var_adv_supported())
                {
                    eddystone_adv_slot_adv_intrvl_set(slot_no, (ble_ecs_adv_intrvl_t *)p_data, false);
                }
                else
                {
                    eddystone_adv_slot_adv_intrvl_set(slot_no, (ble_ecs_adv_intrvl_t *)p_data, true);
                }
                break;

            case BLE_ECS_EVT_RADIO_TX_PWR:
                DEBUG_PRINTF(0, "Variable Tx Support: %d\r\n", ble_eddystone_is_var_tx_pwr_supported());
                if (ble_eddystone_is_var_tx_pwr_supported())
                {
                    eddystone_adv_slot_radio_tx_pwr_set(slot_no, (ble_ecs_radio_tx_pwr_t*)(p_data), false);
                }
                else
                {

                    eddystone_adv_slot_radio_tx_pwr_set(slot_no, (ble_ecs_radio_tx_pwr_t*)(p_data), true);
                }
                break;

            case BLE_ECS_EVT_ADV_TX_PWR:
                //ADVANCED IMPLEMENTATION, NOT IN PLACE YET
                break;

            case BLE_ECS_EVT_LOCK_STATE:
                if (length == 1 && (*p_data == BLE_ECS_LOCK_BYTE_LOCK || *p_data == BLE_ECS_LOCK_BYTE_DISABLE_AUTO_RELOCK))
                {
                    //Do nothing special, allow the write
                }
                else if (length == sizeof(ble_ecs_lock_state_write_t))
                {
                    //0x00 + key[16] : transition to lock state and update the lock code
                    eddystone_security_lock_code_update((p_data)+1);
                    //Only write the lock byte to the characteristic
                    length = 1;
                }
                else
                {
                    //Any invalid values locks the characteristic by default
                    *p_data = BLE_ECS_LOCK_BYTE_LOCK;
                }
                break;

            case BLE_ECS_EVT_RW_ADV_SLOT:
            //client is clearing a slot with an empty array
            if (length == 0)
            {
                slot_data.p_data = NULL;
                slot_data.char_length = length;
            }
            //client is clearing a slot with a single 0 or writing a TLM
            else if (length == 1)
            {
                memcpy(&slot_data.frame_type, p_data, 1);
                slot_data.p_data = NULL;
                slot_data.char_length = length;
            }
            //Other frame type configs
            else
            {
                memcpy(&slot_data.frame_type, p_data, 1);
                slot_data.p_data =  (int8_t*)(p_data + 1);
                slot_data.char_length = length;
            }
                eddystone_adv_slot_rw_buffer_data_set(slot_no, &slot_data);
                break;
            //Long writes to the RW ADV Slot characteristic for configuring an EID frame
            //with an ECDH key exchange
            case BLE_ECS_EVT_RW_ADV_SLOT_PREP:
                long_write_overwrite_flag = true;

                reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
                reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
                reply.params.write.update      = 0;
                reply.params.write.offset      = 0;
                reply.params.write.len         = length;
                reply.params.write.p_data      = NULL;

                err_code = sd_ble_gatts_rw_authorize_reply(m_conn_handle, &reply);
                APP_ERROR_CHECK(err_code);
                break;
            //Long writes to the RW ADV Slot characteristic for configuring an EID frame
            //with an ECDH key exchange
            case BLE_ECS_EVT_RW_ADV_SLOT_EXEC:
                long_write_overwrite_flag = true;

                reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
                reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
                reply.params.write.update      = 0;
                reply.params.write.offset      = 0;
                reply.params.write.len         = length;
                reply.params.write.p_data      = NULL;

                err_code = sd_ble_gatts_rw_authorize_reply(m_conn_handle, &reply);
                APP_ERROR_CHECK(err_code);

                err_code = sd_ble_gatts_value_get(m_conn_handle, p_ecs->rw_adv_slot_handles.value_handle, &value);
                APP_ERROR_CHECK(err_code);

                memcpy(&slot_data.frame_type, value.p_value, 1);
                slot_data.p_data =  (int8_t*)(value.p_value + 1);
                slot_data.char_length = ECS_ADV_SLOT_CHAR_LENGTH_MAX;
                eddystone_adv_slot_rw_buffer_data_set(slot_no, &slot_data);

                break;

            case BLE_ECS_EVT_FACTORY_RESET:
                //ADVANCED IMPLEMENTATION, NOT IN PLACE YET
                break;

            case BLE_ECS_EVT_REMAIN_CNNTBL:
                //ADVANCED IMPLEMENTATION, NOT IN PLACE YET
                break;

            default:
                break;
        }
        reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
        reply.params.write.len = length;
        reply.params.write.p_data = p_data;
    }

    //When the beacon is locked and the client is trying to access characteristics other than
    //the lock characteristic, or when the beacon is unlocked and the client is trying to access
    //the unlock characteristic, deny access
    else if ((!ble_eddystone_is_unlocked() && (evt_type != BLE_ECS_EVT_UNLOCK))
            || (ble_eddystone_is_unlocked() && (evt_type == BLE_ECS_EVT_UNLOCK)))
    {
        reply.params.write.gatt_status = BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED;
        reply.params.write.update      = 1;
        reply.params.write.offset      = 0;
        reply.params.write.len         = 0;
        reply.params.write.p_data      = NULL;
    }

    //When the beacon is locked and the client is trying to access the unlock
    //characteristic accept the write and call the crypto functions to check the validity
    else if (!ble_eddystone_is_unlocked() && (evt_type == BLE_ECS_EVT_UNLOCK))
    {
        uint8_t value_buffer[ECS_AES_KEY_SIZE] = {0};
        memcpy(value_buffer, p_data, length);

        ble_gatts_value_t value = {.len = length, .offset = 0, .p_value = &(value_buffer[0])};

        err_code = sd_ble_gatts_value_set(m_conn_handle, val_handle, &value);
        if (err_code == NRF_SUCCESS)
        {
            reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
            eddystone_security_unlock_verify((value.p_value));
        }
        else
        {
            DEBUG_PRINTF(0, "Ble Gatts Value Set Error \r\n");
            reply.params.write.gatt_status = BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED;
        }
        reply.params.write.len = length;
        reply.params.write.p_data = (const uint8_t *)value.p_value;
    }
    else
    {
        reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
    }

    if ( m_conn_handle != BLE_CONN_HANDLE_INVALID && !long_write_overwrite_flag )
    {
        err_code = sd_ble_gatts_rw_authorize_reply(m_conn_handle, &reply);
        APP_ERROR_CHECK(err_code);
    }
}

/**@brief Function handling all read requests from the Central.
 *
 * @param[in]   p_ecs       Pointer to the eddystone configuration service
 * @param[in]   evt_type    Type of event: corresponding to each characteristic in the service being read from
 * @param[in]   val_handle  Value handle field of the characteristic handle of the characteristic being read from
 *
 */
static void ecs_read_evt_handler(ble_ecs_t        * p_ecs,
                                 ble_ecs_evt_type_t evt_type,
                                 uint16_t           val_handle)
{

    ret_code_t                            err_code;
    ble_gatts_rw_authorize_reply_params_t reply;
    memset(&reply, 0, sizeof(reply));

    reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
    reply.params.read.update      = 1;
    reply.params.read.offset      = 0;

    //Lock state can be read regardless the beacon's lock state
    if (evt_type == BLE_ECS_EVT_LOCK_STATE)
    {
        uint8_t value_buffer[1] = {0};
        ble_gatts_value_t value = {.len = sizeof(value_buffer), .offset = 0, .p_value = &(value_buffer[0])};

        err_code = sd_ble_gatts_value_get(m_conn_handle, val_handle, &value);
        APP_ERROR_CHECK(err_code);

        reply.params.read.len = value.len;
        reply.params.read.p_data = (const uint8_t *)value.p_value;
    }

    else if (ble_eddystone_is_unlocked() && (evt_type != BLE_ECS_EVT_UNLOCK))
    {
        bool        override_flag = false; /*When true, overrides a direct read of the characteristic:
                                           used for per slot properties*/
        uint8_t     slot_no = ble_eddystone_active_slot_get();
        DEBUG_PRINTF(0,"ACTIVE SLOT: %d \r\n",slot_no);

        switch (evt_type)
        {
            case BLE_ECS_EVT_BRDCST_CAP:
                //Go straight to the characteristic
                break;

            case BLE_ECS_EVT_ACTIVE_SLOT:
                //Go straight to the characteristic
                break;

            case BLE_ECS_EVT_ADV_INTRVL:
                override_flag = true;
                ble_ecs_adv_intrvl_t interval;

                eddystone_adv_slot_adv_intrvl_get(slot_no, &interval);
                reply.params.read.len = sizeof(ble_ecs_adv_intrvl_t);
                reply.params.read.p_data = (const uint8_t *)(&interval);
                reply.params.read.gatt_status = BLE_GATT_STATUS_SUCCESS;
                break;

            case BLE_ECS_EVT_RADIO_TX_PWR:
                override_flag = true;
                ble_ecs_radio_tx_pwr_t  tx_pwr;

                eddystone_adv_slot_radio_tx_pwr_get(slot_no, &tx_pwr);
                reply.params.read.len = sizeof(ble_ecs_radio_tx_pwr_t);
                reply.params.read.p_data = (const uint8_t *)(&tx_pwr);
                reply.params.read.gatt_status = BLE_GATT_STATUS_SUCCESS;
                break;

            case BLE_ECS_EVT_ADV_TX_PWR:
                //ADVANCED OPTION, CURRENTLY NOT IMPLEMENTED
                break;

            case BLE_ECS_EVT_UNLOCK:
                //Go straight to the characteristic
                break;

            case BLE_ECS_EVT_PUBLIC_ECDH_KEY:
                //Go straight to the characteristic
                break;

            case BLE_ECS_EVT_EID_ID_KEY:
                override_flag = true;
                ble_ecs_eid_id_key_t eid_id_key;

                reply.params.read.gatt_status = BLE_GATT_STATUS_SUCCESS;

                if(eddystone_adv_slot_eid_id_key_get(slot_no, &eid_id_key) == NRF_ERROR_INVALID_STATE)
                {
                    reply.params.read.gatt_status = BLE_GATT_STATUS_ATTERR_READ_NOT_PERMITTED;
                }
                reply.params.read.len = sizeof(ble_ecs_eid_id_key_t);
                reply.params.read.p_data = (uint8_t*)eid_id_key.key;
                break;

            case BLE_ECS_EVT_RW_ADV_SLOT:
                override_flag = true;
                ble_ecs_rw_adv_slot_t  slot_data;
                uint8_t buffer[ECS_ADV_SLOT_CHAR_LENGTH_MAX];

                eddystone_adv_slot_rw_buffer_data_get(slot_no, &slot_data);
                buffer[0] = slot_data.frame_type;
                //If non-empty slot
                if (slot_data.char_length > 0 )
                {
                    memcpy(&(buffer[1]), slot_data.p_data, slot_data.char_length - 1); //subtract frametype
                }
                reply.params.read.len = slot_data.char_length;
                reply.params.read.p_data = buffer;
                reply.params.read.gatt_status = BLE_GATT_STATUS_SUCCESS;
                break;

            case BLE_ECS_EVT_REMAIN_CNNTBL:
                break;

            default:
                break;
        }

        if (!override_flag)
        {
            uint8_t value_buffer[ECS_ADV_SLOT_CHAR_LENGTH_MAX] = {0};
            ble_gatts_value_t value = {.len = sizeof(value_buffer), .offset = 0, .p_value = &(value_buffer[0])};

            err_code = sd_ble_gatts_value_get(m_conn_handle, val_handle, &value);
            APP_ERROR_CHECK(err_code);

            reply.params.read.len = value.len;
            reply.params.read.p_data = (const uint8_t *)value.p_value;
            reply.params.read.gatt_status = BLE_GATT_STATUS_SUCCESS;
        }
    }
        //When the beacon is locked and the client is trying to access characteristics other than
        //the lock characteristic, or when the beacon is unlocked and the client is trying to access
        //the unlock characteristic, deny access
    else if ((!ble_eddystone_is_unlocked() && (evt_type != BLE_ECS_EVT_UNLOCK))
            || (ble_eddystone_is_unlocked() && (evt_type == BLE_ECS_EVT_UNLOCK)))
    {
        reply.params.read.gatt_status = BLE_GATT_STATUS_ATTERR_READ_NOT_PERMITTED;
        reply.params.read.update      = 1;
        reply.params.read.offset      = 0;
        reply.params.read.len         = 0;
        reply.params.read.p_data      = NULL;
    }

    //When the beacon is locked and the client is trying to access the unlock
    //characteristic accept the read and call the cryptography function to check the validity
    else if (!ble_eddystone_is_unlocked() && (evt_type == BLE_ECS_EVT_UNLOCK))
    {
        uint8_t key_buff[ECS_AES_KEY_SIZE];
        uint32_t err_code;

        eddystone_security_random_challenge_generate(key_buff);
        err_code = eddystone_security_unlock_prepare(key_buff);
        APP_ERROR_CHECK(err_code);

        reply.params.read.gatt_status = BLE_GATT_STATUS_SUCCESS;
        reply.params.read.update      = 1;
        reply.params.read.offset      = 0;
        reply.params.read.len         = ECS_AES_KEY_SIZE;
        reply.params.read.p_data      = key_buff;
    }

    if ( m_conn_handle != BLE_CONN_HANDLE_INVALID )
    {
        err_code = sd_ble_gatts_rw_authorize_reply(m_conn_handle, &reply);
        APP_ERROR_CHECK(err_code);
    }
}
/**@brief Initialize the ECS with initial values for the characteristics and other necessary modules */
static void services_and_modules_init(void)
{
    ret_code_t err_code;
    ble_ecs_init_t ecs_init;
    ble_ecs_init_params_t init_params;
    int8_t tx_powers[ECS_NUM_OF_SUPORTED_TX_POWER] = ECS_SUPPORTED_TX_POWER;

    /*Init the broadcast capabilities characteristic*/
    memset(&init_params.brdcst_cap, 0, sizeof(init_params.brdcst_cap));
    init_params.brdcst_cap.vers_byte            = EDDYSTONE_SPEC_VERSION_BYTE;
    init_params.brdcst_cap.max_supp_total_slots = APP_MAX_ADV_SLOTS;
    init_params.brdcst_cap.max_supp_eid_slots   = APP_MAX_EID_SLOTS;
    init_params.brdcst_cap.cap_bitfield         = ( (APP_IS_VARIABLE_ADV_SUPPORTED << ECS_BRDCST_VAR_ADV_SUPPORTED_Pos)
                                                  | (APP_IS_VARIABLE_TX_POWER_SUPPORTED << ECS_BRDCST_VAR_TX_POWER_SUPPORTED_Pos))
                                                  & (ECS_BRDCST_VAR_RFU_MASK);
    init_params.brdcst_cap.supp_frame_types     = ( (ECS_FRAME_TYPE_URL_SUPPORTED_Yes << ECS_FRAME_TYPE_URL_SUPPORTED_Pos)
                                                  | (ECS_FRAME_TYPE_UID_SUPPORTED_Yes << ECS_FRAME_TYPE_UID_SUPPORTED_Pos)
                                                  | (ECS_FRAME_TYPE_TLM_SUPPORTED_Yes << ECS_FRAME_TYPE_TLM_SUPPORTED_Pos)
                                                  | (ECS_FRAME_TYPE_EID_SUPPORTED_Yes << ECS_FRAME_TYPE_EID_SUPPORTED_Pos))
                                                  & (ECS_FRAME_TYPE_RFU_MASK);
    memcpy(init_params.brdcst_cap.supp_radio_tx_power, tx_powers, ECS_NUM_OF_SUPORTED_TX_POWER);

    /*Init the active slots characteristic*/
    init_params.active_slot = 0;

    /*Init the advertising intervals characteristic*/
    init_params.adv_intrvl = DEFAULT_NON_CONNECTABLE_ADV_INTERVAL_MS;

    /*Init the radio tx power characteristic*/
    init_params.adv_tx_pwr = 0x00;

    /*Init the radio tx power characteristic*/
    init_params.radio_tx_pwr = 0x00;

    /*Init the lock state characteristic*/
    init_params.lock_state.read = BLE_ECS_LOCK_STATE_LOCKED;

    uint8_t eddystone_default_data[] = DEFAULT_FRAME_DATA;
    init_params.rw_adv_slot.frame_type = (eddystone_frame_type_t)(DEFAULT_FRAME_TYPE);
    init_params.rw_adv_slot.p_data = (int8_t *)(eddystone_default_data);   //Without frametype
    init_params.rw_adv_slot.char_length = sizeof(eddystone_default_data) + 1;

    init_params.factory_reset = 0;
    init_params.remain_cnntbl.r_is_non_connectable_supported = 1;

    //Initialize evt handlers and the service
    memset(&ecs_init, NULL, sizeof(ecs_init));
    ecs_init.write_evt_handler = ecs_write_evt_handler;
    ecs_init.read_evt_handler = ecs_read_evt_handler;
    ecs_init.p_init_vals = &(init_params);

    err_code = ble_ecs_init(&m_ble_ecs, &ecs_init);
    APP_ERROR_CHECK(err_code);

    //Initialize the slots with the initial values of the characteristics
    eddystone_adv_slots_init(&ecs_init);

    //Initialize the security module
    eddystone_security_init_t security_init =
    {
        .msg_cb = ble_eddystone_security_cb,
        .eid_slots_max = APP_MAX_EID_SLOTS
    };
    err_code = eddystone_security_init(&security_init);
    APP_ERROR_CHECK(err_code);
}

void eddystone_ble_init()
{
    ble_stack_init();
    /* Enable FPU again due to SD bug issue */
    #if (__FPU_USED == 1)
    SCB->CPACR |= (3UL << 20) | (3UL << 22);
    __DSB();
    __ISB();
    #endif

    gap_params_init();
    conn_params_init();

    services_and_modules_init();
    eddystone_advertising_manager_init(m_ble_ecs.uuid_type);
}
