#include "ble_ecs.h"
#include <string.h>
#include "endian_convert.h"



#define GATT_DEBUG

#ifdef GATT_DEBUG
    #include "SEGGER_RTT.h"
    #define DEBUG_PRINTF SEGGER_RTT_printf
#else
    #define DEBUG_PRINTF(...)
#endif

/*ECS UUIDs*/
#define BLE_UUID_ECS_BRDCST_CAP_CHAR            0x7501
#define BLE_UUID_ECS_ACTIVE_SLOT_CHAR           0x7502
#define BLE_UUID_ECS_ADV_INTRVL_CHAR            0x7503
#define BLE_UUID_ECS_RADIO_TX_PWR_CHAR          0x7504
#define BLE_UUID_ECS_ADV_TX_PWR_CHAR            0x7505
#define BLE_UUID_ECS_LOCK_STATE_CHAR            0x7506
#define BLE_UUID_ECS_UNLOCK_CHAR                0x7507
#define BLE_UUID_ECS_PUBLIC_ECDH_KEY_CHAR       0x7508
#define BLE_UUID_ECS_EID_ID_KEY_CHAR            0x7509
#define BLE_UUID_ECS_RW_ADV_SLOT_CHAR           0x750A
#define BLE_UUID_ECS_FACTORY_RESET_CHAR         0x750B
#define BLE_UUID_ECS_REMAIN_CNNTBL_CHAR         0x750C

#define ECS_BASE_UUID                       \
{{0x95, 0xE2, 0xED, 0xEB, 0x1B, 0xA0, 0x39, 0x8A, 0xDF, 0x4B, 0xD3, 0x8E, 0x00, 0x00, 0xC8, 0xA3}}
//A3C8XXXX-8ED3-4BDF-8A39-A01BEBEDE295

//According to the spec, there are 6 bytes of data in addition to the supported_radio_tx_power array
#define BLE_ECS_BRDCST_CAP_LEN                  (ECS_NUM_OF_SUPORTED_TX_POWER + 6)

/*Memory block for EID Long writes*/
#define EID_BUFF_SIZE 64
static uint8_t m_eid_mem[EID_BUFF_SIZE] = {0};
static ble_user_mem_block_t    m_eid_mem_block = {.p_mem = m_eid_mem, .len = EID_BUFF_SIZE};

/**@brief Function for handling the @ref BLE_GAP_EVT_CONNECTED event from the S132 SoftDevice.
 *
 * @param[in] p_ecs     Eddystone Configuration Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_connect(ble_ecs_t * p_ecs, ble_evt_t * p_ble_evt)
{
    p_ecs->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
}


/**@brief Function for handling the @ref BLE_GAP_EVT_DISCONNECTED event from the S132 SoftDevice.
 *
 * @param[in] p_ecs     Eddystone Configuration Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_disconnect(ble_ecs_t * p_ecs, ble_evt_t * p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    p_ecs->conn_handle = BLE_CONN_HANDLE_INVALID;
}


/**@brief Function for handling the @ref BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST: BLE_GATTS_AUTHORIZE_TYPE_WRITE event from the S132 SoftDevice.
 *
 * @param[in] p_ecs     Eddystone Configuration Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_write(ble_ecs_t * p_ecs, ble_evt_t * p_ble_evt)
{
    //ble_gatts_evt_rw_authorize_request_t * p_evt_rw_auth = &p_ble_evt->evt.gatts_evt.params.authorize_request;
    ble_gatts_evt_write_t * p_evt_write = &p_ble_evt->evt.gatts_evt.params.authorize_request.request.write;


    if (p_ecs->write_evt_handler != NULL)
    {
        if (p_evt_write->handle == p_ecs->active_slot_handles.value_handle)
        {
            p_ecs->write_evt_handler(p_ecs, BLE_ECS_EVT_ACTIVE_SLOT, p_evt_write->handle, p_evt_write->data, p_evt_write->len);
        }
        else if (p_evt_write->handle == p_ecs->adv_intrvl_handles.value_handle)
        {
            p_ecs->write_evt_handler(p_ecs, BLE_ECS_EVT_ADV_INTRVL, p_evt_write->handle, p_evt_write->data, p_evt_write->len);
        }
        else if (p_evt_write->handle == p_ecs->radio_tx_pwr_handles.value_handle)
        {
            p_ecs->write_evt_handler(p_ecs, BLE_ECS_EVT_RADIO_TX_PWR, p_evt_write->handle, p_evt_write->data, p_evt_write->len);
        }
        else if (p_evt_write->handle == p_ecs->adv_tx_pwr_handles.value_handle)
        {
            p_ecs->write_evt_handler(p_ecs, BLE_ECS_EVT_ADV_TX_PWR, p_evt_write->handle, p_evt_write->data, p_evt_write->len);
        }
        else if (p_evt_write->handle == p_ecs->lock_state_handles.value_handle)
        {
            p_ecs->write_evt_handler(p_ecs, BLE_ECS_EVT_LOCK_STATE, p_evt_write->handle, p_evt_write->data, p_evt_write->len);
        }
        else if (p_evt_write->handle == p_ecs->unlock_handles.value_handle)
        {
            p_ecs->write_evt_handler(p_ecs, BLE_ECS_EVT_UNLOCK, p_evt_write->handle, p_evt_write->data, p_evt_write->len);
        }
        //BLE_GATTS_OP_PREP_WRITE_REQ & BLE_GATTS_OP_EXEC_WRITE_REQ_NOW are for long writes to the RW ADV slot characteristic
        else if (p_evt_write->op == BLE_GATTS_OP_PREP_WRITE_REQ)
        {
            p_ecs->write_evt_handler(p_ecs, BLE_ECS_EVT_RW_ADV_SLOT_PREP, p_evt_write->handle, p_evt_write->data, p_evt_write->len);
        }
        else if (p_evt_write->op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW)
        {
            p_ecs->write_evt_handler(p_ecs, BLE_ECS_EVT_RW_ADV_SLOT_EXEC, p_evt_write->handle, p_evt_write->data, p_evt_write->len);
        }
        else if (p_evt_write->handle == p_ecs->rw_adv_slot_handles.value_handle)
        {
            p_ecs->write_evt_handler(p_ecs, BLE_ECS_EVT_RW_ADV_SLOT, p_evt_write->handle, p_evt_write->data, p_evt_write->len);
        }
        else if (p_evt_write->handle == p_ecs->factory_reset_handles.value_handle)
        {
            p_ecs->write_evt_handler(p_ecs, BLE_ECS_EVT_FACTORY_RESET, p_evt_write->handle, p_evt_write->data, p_evt_write->len);
        }
        else if (p_evt_write->handle == p_ecs->remain_cnntbl_handles.value_handle)
        {
            p_ecs->write_evt_handler(p_ecs, BLE_ECS_EVT_REMAIN_CNNTBL, p_evt_write->handle, p_evt_write->data, p_evt_write->len);
        }
        else
        {
            // Do Nothing. This event is not relevant for this service.
        }
    }
}

/**@brief Function for handling the @ref BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST: BLE_GATTS_AUTHORIZE_TYPE_READ event from the S132 SoftDevice.
 *
 * @param[in] p_ecs     Eddystone Configuration Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_read(ble_ecs_t * p_ecs, ble_evt_t * p_ble_evt)
{
    ble_gatts_evt_read_t * p_evt_read = &p_ble_evt->evt.gatts_evt.params.authorize_request.request.read;

    if (p_evt_read->handle == p_ecs->brdcst_cap_handles.value_handle)
    {
        p_ecs->read_evt_handler(p_ecs, BLE_ECS_EVT_BRDCST_CAP, p_evt_read->handle);
    }
    else if (p_evt_read->handle == p_ecs->active_slot_handles.value_handle)
    {
        p_ecs->read_evt_handler(p_ecs, BLE_ECS_EVT_ACTIVE_SLOT, p_evt_read->handle);
    }
    else if (p_evt_read->handle == p_ecs->adv_intrvl_handles.value_handle)
    {
        p_ecs->read_evt_handler(p_ecs, BLE_ECS_EVT_ADV_INTRVL, p_evt_read->handle);
    }
    else if (p_evt_read->handle == p_ecs->radio_tx_pwr_handles.value_handle)
    {
        p_ecs->read_evt_handler(p_ecs, BLE_ECS_EVT_RADIO_TX_PWR, p_evt_read->handle);
    }
    else if (p_evt_read->handle == p_ecs->adv_tx_pwr_handles.value_handle)
    {
        p_ecs->read_evt_handler(p_ecs, BLE_ECS_EVT_ADV_TX_PWR, p_evt_read->handle);
    }
    else if (p_evt_read->handle == p_ecs->unlock_handles.value_handle)
    {
        p_ecs->read_evt_handler(p_ecs, BLE_ECS_EVT_UNLOCK, p_evt_read->handle);
    }
    else if (p_evt_read->handle == p_ecs->pub_ecdh_key_handles.value_handle)
    {
        p_ecs->read_evt_handler(p_ecs, BLE_ECS_EVT_PUBLIC_ECDH_KEY, p_evt_read->handle);
    }
    else if (p_evt_read->handle == p_ecs->eid_id_key_handles.value_handle)
    {
        p_ecs->read_evt_handler(p_ecs, BLE_ECS_EVT_EID_ID_KEY, p_evt_read->handle);
    }
    else if (p_evt_read->handle == p_ecs->rw_adv_slot_handles.value_handle)
    {
        p_ecs->read_evt_handler(p_ecs, BLE_ECS_EVT_RW_ADV_SLOT, p_evt_read->handle);
    }
    else
    {
        // Do Nothing. This event is not relevant for this service.
    }
}

void ble_ecs_on_ble_evt(ble_ecs_t * p_ecs, ble_evt_t * p_ble_evt)
{
    uint32_t err_code;

    if ((p_ecs == NULL) || (p_ble_evt == NULL))
    {
        return;
    }

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_ecs, p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_ecs, p_ble_evt);
            break;

        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
            if (p_ble_evt->evt.gatts_evt.params.authorize_request.type == BLE_GATTS_AUTHORIZE_TYPE_READ)
            {
                on_read(p_ecs, p_ble_evt);
            }
            else if (p_ble_evt->evt.gatts_evt.params.authorize_request.type == BLE_GATTS_AUTHORIZE_TYPE_WRITE)
            {
                if (p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.op == BLE_GATTS_OP_PREP_WRITE_REQ)
                {
                    DEBUG_PRINTF(0,"PREP_WRITE_REQUEST \r\n",0);
                }
                else if (p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW)
                {
                    DEBUG_PRINTF(0,"EXEC_WRITE_REQUEST \r\n",0);
                }
                on_write(p_ecs, p_ble_evt);
            }
            else
            {
                //BLE_GATTS_AUTHORIZE_TYPE_INVALID TODO: Report Error?
            }
            break;
        //BLE_EVT_USER_MEM_REQUEST & BLE_EVT_USER_MEM_RELEASE are for long writes to the RW ADV slot characteristic
        case BLE_EVT_USER_MEM_REQUEST:
            err_code = sd_ble_user_mem_reply(p_ecs->conn_handle, &m_eid_mem_block);
            DEBUG_PRINTF(0,"USER_MEM_REQUEST: error: %d \r\n", err_code);
            break;

        case BLE_EVT_USER_MEM_RELEASE:
            DEBUG_PRINTF(0,"USER_MEM_RELEASE\r\n", 0);
            break;
        default:
            // No implementation needed.
            break;
    }
}

/**@brief Function for adding braodcast capability characteristic.
 *
 * @param[in] p_ecs       Eddystone Configuration Service structure.
 * @param[in] p_ecs_init  Information needed to initialize the service.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t brdcst_cap_char_add(ble_ecs_t * p_ecs, const ble_ecs_init_t * p_ecs_init)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read          = 1;
    char_md.p_char_user_desc         = NULL;
    char_md.p_char_pf                = NULL;
    char_md.p_user_desc_md           = NULL;
    char_md.p_cccd_md                = NULL;
    char_md.p_sccd_md                = NULL;

    ble_uuid.type = p_ecs->uuid_type;
    ble_uuid.uuid = BLE_UUID_ECS_BRDCST_CAP_CHAR;

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 1;
    attr_md.wr_auth = 0;
    attr_md.vlen    = 1;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    ble_ecs_brdcst_cap_t temp = p_ecs_init->p_init_vals->brdcst_cap;
    temp.supp_frame_types = BYTES_SWAP_16BIT(temp.supp_frame_types);

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = BLE_ECS_BRDCST_CAP_LEN;
    attr_char_value.init_offs = 0;
    attr_char_value.p_value   = (uint8_t *)(&temp);
    attr_char_value.max_len   = BLE_ECS_BRDCST_CAP_LEN;

    return sd_ble_gatts_characteristic_add(p_ecs->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_ecs->brdcst_cap_handles);
}

/**@brief Function for adding active slot characteristic.
 *
 * @param[in] p_ecs       Eddystone Configuration Service structure.
 * @param[in] p_ecs_init  Information needed to initialize the service.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t active_slot_char_add(ble_ecs_t * p_ecs, const ble_ecs_init_t * p_ecs_init)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read          = 1;
    char_md.char_props.write         = 1;
    char_md.p_char_user_desc         = NULL;
    char_md.p_char_pf                = NULL;
    char_md.p_user_desc_md           = NULL;
    char_md.p_cccd_md                = NULL;
    char_md.p_sccd_md                = NULL;

    ble_uuid.type = p_ecs->uuid_type;
    ble_uuid.uuid = BLE_UUID_ECS_ACTIVE_SLOT_CHAR;

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 1;
    attr_md.wr_auth = 1;
    attr_md.vlen    = 0;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = sizeof(ble_ecs_active_slot_t);
    attr_char_value.init_offs = 0;
    attr_char_value.p_value   = (uint8_t *)(&(p_ecs_init->p_init_vals->active_slot));
    attr_char_value.max_len   = sizeof(ble_ecs_active_slot_t);

    return sd_ble_gatts_characteristic_add(p_ecs->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_ecs->active_slot_handles);
}

/**@brief Function for adding advertising interval characteristic.
 *
 * @param[in] p_ecs       Eddystone Configuration Service structure.
 * @param[in] p_ecs_init  Information needed to initialize the service.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t adv_intrvl_char_add(ble_ecs_t * p_ecs, const ble_ecs_init_t * p_ecs_init)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read          = 1;
    char_md.char_props.write         = 1;
    char_md.p_char_user_desc         = NULL;
    char_md.p_char_pf                = NULL;
    char_md.p_user_desc_md           = NULL;
    char_md.p_cccd_md                = NULL;
    char_md.p_sccd_md                = NULL;

    ble_uuid.type = p_ecs->uuid_type;
    ble_uuid.uuid = BLE_UUID_ECS_ADV_INTRVL_CHAR;

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 1;
    attr_md.wr_auth = 1;
    attr_md.vlen    = 0;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    ble_ecs_adv_intrvl_t temp = p_ecs_init->p_init_vals->adv_intrvl;
    temp = BYTES_SWAP_16BIT(temp);

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = sizeof(ble_ecs_adv_intrvl_t);
    attr_char_value.init_offs = 0;
    attr_char_value.p_value   = (uint8_t *)(&temp);
    attr_char_value.max_len   = sizeof(ble_ecs_adv_intrvl_t);

    return sd_ble_gatts_characteristic_add(p_ecs->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_ecs->adv_intrvl_handles);
}

/**@brief Function for adding radio tx power characteristic.
 *
 * @param[in] p_ecs       Eddystone Configuration Service structure.
 * @param[in] p_ecs_init  Information needed to initialize the service.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t radio_tx_pwr_char_add(ble_ecs_t * p_ecs, const ble_ecs_init_t * p_ecs_init)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read          = 1;
    char_md.char_props.write         = 1;
    char_md.p_char_user_desc         = NULL;
    char_md.p_char_pf                = NULL;
    char_md.p_user_desc_md           = NULL;
    char_md.p_cccd_md                = NULL;
    char_md.p_sccd_md                = NULL;

    ble_uuid.type = p_ecs->uuid_type;
    ble_uuid.uuid = BLE_UUID_ECS_RADIO_TX_PWR_CHAR;

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 1;
    attr_md.wr_auth = 1;
    attr_md.vlen    = 0;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = sizeof(ble_ecs_radio_tx_pwr_t);
    attr_char_value.init_offs = 0;
    attr_char_value.p_value   = (uint8_t *)(&(p_ecs_init->p_init_vals->radio_tx_pwr));
    attr_char_value.max_len   = sizeof(ble_ecs_radio_tx_pwr_t);

    return sd_ble_gatts_characteristic_add(p_ecs->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_ecs->radio_tx_pwr_handles);
}

/**@brief Function for adding radio tx power characteristic.
 *
 * @param[in] p_ecs       Eddystone Configuration Service structure.
 * @param[in] p_ecs_init  Information needed to initialize the service.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t adv_tx_pwr_char_add(ble_ecs_t * p_ecs, const ble_ecs_init_t * p_ecs_init)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read          = 1;
    char_md.char_props.write         = 1;
    char_md.p_char_user_desc         = NULL;
    char_md.p_char_pf                = NULL;
    char_md.p_user_desc_md           = NULL;
    char_md.p_cccd_md                = NULL;
    char_md.p_sccd_md                = NULL;

    ble_uuid.type = p_ecs->uuid_type;
    ble_uuid.uuid = BLE_UUID_ECS_ADV_TX_PWR_CHAR;

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 1;
    attr_md.wr_auth = 1;
    attr_md.vlen    = 0;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = sizeof(ble_ecs_adv_tx_pwr_t);
    attr_char_value.init_offs = 0;
    attr_char_value.p_value   = (uint8_t *)(&(p_ecs_init->p_init_vals->adv_tx_pwr));
    attr_char_value.max_len   = sizeof(ble_ecs_adv_tx_pwr_t);

    return sd_ble_gatts_characteristic_add(p_ecs->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_ecs->adv_tx_pwr_handles);
}

/**@brief Function for adding lock state characteristic.
 *
 * @param[in] p_ecs       Eddystone Configuration Service structure.
 * @param[in] p_ecs_init  Information needed to initialize the service.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t lock_state_char_add(ble_ecs_t * p_ecs, const ble_ecs_init_t * p_ecs_init)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read          = 1;
    char_md.char_props.write         = 1;
    char_md.p_char_user_desc         = NULL;
    char_md.p_char_pf                = NULL;
    char_md.p_user_desc_md           = NULL;
    char_md.p_cccd_md                = NULL;
    char_md.p_sccd_md                = NULL;

    ble_uuid.type = p_ecs->uuid_type;
    ble_uuid.uuid = BLE_UUID_ECS_LOCK_STATE_CHAR;

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 1;
    attr_md.vlen    = 1;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = 1;
    attr_char_value.init_offs = 0;
    attr_char_value.p_value   = (uint8_t *)(&(p_ecs_init->p_init_vals->lock_state.read));
    attr_char_value.max_len   = 17;

    return sd_ble_gatts_characteristic_add(p_ecs->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_ecs->lock_state_handles);
}

/**@brief Function for adding unlock characteristic.
 *
 * @param[in] p_ecs       Eddystone Configuration Service structure.
 * @param[in] p_ecs_init  Information needed to initialize the service.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t unlock_char_add(ble_ecs_t * p_ecs, const ble_ecs_init_t * p_ecs_init)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read          = 1;
    char_md.char_props.write         = 1;
    char_md.p_char_user_desc         = NULL;
    char_md.p_char_pf                = NULL;
    char_md.p_user_desc_md           = NULL;
    char_md.p_cccd_md                = NULL;
    char_md.p_sccd_md                = NULL;

    ble_uuid.type = p_ecs->uuid_type;
    ble_uuid.uuid = BLE_UUID_ECS_UNLOCK_CHAR;

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 1;
    attr_md.wr_auth = 1;
    attr_md.vlen    = 1;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    uint8_t init_val = 0;

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = 1;
    attr_char_value.init_offs = 0;
    attr_char_value.p_value   = &init_val;
    attr_char_value.max_len   = ECS_AES_KEY_SIZE;

    return sd_ble_gatts_characteristic_add(p_ecs->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_ecs->unlock_handles);
}

/**@brief Function for adding public ECDH key characteristic.
 *
 * @param[in] p_ecs       Eddystone Configuration Service structure.
 * @param[in] p_ecs_init  Information needed to initialize the service.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t pub_ecdh_key_char_add(ble_ecs_t * p_ecs, const ble_ecs_init_t * p_ecs_init)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read          = 1;
    char_md.p_char_user_desc         = NULL;
    char_md.p_char_pf                = NULL;
    char_md.p_user_desc_md           = NULL;
    char_md.p_cccd_md                = NULL;
    char_md.p_sccd_md                = NULL;

    ble_uuid.type = p_ecs->uuid_type;
    ble_uuid.uuid = BLE_UUID_ECS_PUBLIC_ECDH_KEY_CHAR;

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 1;
    attr_md.wr_auth = 0;
    attr_md.vlen    = 1;

    memset(&attr_char_value, 0, sizeof(attr_char_value));
    uint8_t init_val = 0;
    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = 1;
    attr_char_value.init_offs = 0;
    attr_char_value.p_value   = &init_val;
    attr_char_value.max_len   = ECS_ECDH_KEY_SIZE;

    return sd_ble_gatts_characteristic_add(p_ecs->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_ecs->pub_ecdh_key_handles);
}

/**@brief Function for adding EID ID key characteristic.
 *
 * @param[in] p_ecs       Eddystone Configuration Service structure.
 * @param[in] p_ecs_init  Information needed to initialize the service.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t eid_id_key_char_add(ble_ecs_t * p_ecs, const ble_ecs_init_t * p_ecs_init)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read          = 1;
    char_md.p_char_user_desc         = NULL;
    char_md.p_char_pf                = NULL;
    char_md.p_user_desc_md           = NULL;
    char_md.p_cccd_md                = NULL;
    char_md.p_sccd_md                = NULL;

    ble_uuid.type = p_ecs->uuid_type;
    ble_uuid.uuid = BLE_UUID_ECS_EID_ID_KEY_CHAR;

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 1;
    attr_md.wr_auth = 0;
    attr_md.vlen    = 1;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    uint8_t init_val = 0;

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = 1;
    attr_char_value.init_offs = 0;
    attr_char_value.p_value   = &init_val;
    attr_char_value.max_len   = ECS_AES_KEY_SIZE;

    return sd_ble_gatts_characteristic_add(p_ecs->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_ecs->eid_id_key_handles);
}

/**@brief Function for adding readwrite ADV slot characteristic.
 *
 * @param[in] p_ecs       Eddystone Configuration Service structure.
 * @param[in] p_ecs_init  Information needed to initialize the service.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t rw_adv_slot_char_add(ble_ecs_t * p_ecs, const ble_ecs_init_t * p_ecs_init)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read          = 1;
    char_md.char_props.write         = 1;
    char_md.p_char_user_desc         = NULL;
    char_md.p_char_pf                = NULL;
    char_md.p_user_desc_md           = NULL;
    char_md.p_cccd_md                = NULL;
    char_md.p_sccd_md                = NULL;

    ble_uuid.type = p_ecs->uuid_type;
    ble_uuid.uuid = BLE_UUID_ECS_RW_ADV_SLOT_CHAR;

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 1;
    attr_md.wr_auth = 1;
    attr_md.vlen    = 1;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    uint8_t temp[ECS_ADV_SLOT_CHAR_LENGTH_MAX] = {0};
    memcpy(temp, &(p_ecs_init->p_init_vals->rw_adv_slot.frame_type), EDDYSTONE_FRAME_TYPE_LENGTH);
    memcpy(&(temp[1]),
           p_ecs_init->p_init_vals->rw_adv_slot.p_data,
           (p_ecs_init->p_init_vals->rw_adv_slot.char_length - EDDYSTONE_FRAME_TYPE_LENGTH));

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = p_ecs_init->p_init_vals->rw_adv_slot.char_length;
    attr_char_value.init_offs = 0;
    attr_char_value.p_value   = temp;
    attr_char_value.max_len   = ECS_ADV_SLOT_CHAR_LENGTH_MAX;

    return sd_ble_gatts_characteristic_add(p_ecs->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_ecs->rw_adv_slot_handles);
}

/**@brief Function for adding factory reset characteristic.
 *
 * @param[in] p_ecs       Eddystone Configuration Service structure.
 * @param[in] p_ecs_init  Information needed to initialize the service.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t factory_reset_char_add(ble_ecs_t * p_ecs, const ble_ecs_init_t * p_ecs_init)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.write         = 1;
    char_md.p_char_user_desc         = NULL;
    char_md.p_char_pf                = NULL;
    char_md.p_user_desc_md           = NULL;
    char_md.p_cccd_md                = NULL;
    char_md.p_sccd_md                = NULL;

    ble_uuid.type = p_ecs->uuid_type;
    ble_uuid.uuid = BLE_UUID_ECS_FACTORY_RESET_CHAR;

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 1;
    attr_md.vlen    = 0;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    ble_ecs_factory_reset_t temp = p_ecs_init->p_init_vals->factory_reset;

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = sizeof(temp);
    attr_char_value.init_offs = 0;
    attr_char_value.p_value   = (uint8_t *)(&(temp));
    attr_char_value.max_len   = sizeof(temp);

    return sd_ble_gatts_characteristic_add(p_ecs->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_ecs->factory_reset_handles);
}

/**@brief Function for adding remain connectable characteristic.
 *
 * @param[in] p_ecs       Eddystone Configuration Service structure.
 * @param[in] p_ecs_init  Information needed to initialize the service.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t remain_cnntbl_char_add(ble_ecs_t * p_ecs, const ble_ecs_init_t * p_ecs_init)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.write         = 1;
    char_md.char_props.read          = 1;
    char_md.p_char_user_desc         = NULL;
    char_md.p_char_pf                = NULL;
    char_md.p_user_desc_md           = NULL;
    char_md.p_cccd_md                = NULL;
    char_md.p_sccd_md                = NULL;

    ble_uuid.type = p_ecs->uuid_type;
    ble_uuid.uuid = BLE_UUID_ECS_REMAIN_CNNTBL_CHAR;

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 1;
    attr_md.vlen    = 0;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    uint8_t temp = p_ecs_init->p_init_vals->remain_cnntbl.r_is_non_connectable_supported;

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = sizeof(temp);
    attr_char_value.init_offs = 0;
    attr_char_value.p_value   = (uint8_t *)(&(temp));
    attr_char_value.max_len   = sizeof(temp);

    return sd_ble_gatts_characteristic_add(p_ecs->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_ecs->remain_cnntbl_handles);
}

uint32_t ble_ecs_init(ble_ecs_t * p_ecs, const ble_ecs_init_t * p_ecs_init)
{
    uint32_t      err_code;
    ble_uuid_t    ble_uuid;
    ble_uuid128_t ecs_base_uuid = ECS_BASE_UUID;

    VERIFY_PARAM_NOT_NULL(p_ecs);
    VERIFY_PARAM_NOT_NULL(p_ecs_init);

    // Initialize the service structure.
    p_ecs->conn_handle                        = BLE_CONN_HANDLE_INVALID;
    p_ecs->write_evt_handler                  = p_ecs_init->write_evt_handler;
    p_ecs->read_evt_handler                   = p_ecs_init->read_evt_handler;

    // Add a custom base UUID.
    err_code = sd_ble_uuid_vs_add(&ecs_base_uuid, &p_ecs->uuid_type);
    VERIFY_SUCCESS(err_code);

    ble_uuid.type = p_ecs->uuid_type;
    ble_uuid.uuid = BLE_UUID_ECS_SERVICE;

    // Add the service.
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &ble_uuid,
                                        &p_ecs->service_handle);
    VERIFY_SUCCESS(err_code);

    /*Adding chracteristics*/
    err_code = brdcst_cap_char_add(p_ecs, p_ecs_init);
    VERIFY_SUCCESS(err_code);

    err_code = active_slot_char_add(p_ecs, p_ecs_init);
    VERIFY_SUCCESS(err_code);

    err_code = adv_intrvl_char_add(p_ecs, p_ecs_init);
    VERIFY_SUCCESS(err_code);

    err_code = radio_tx_pwr_char_add(p_ecs, p_ecs_init);
    VERIFY_SUCCESS(err_code);

    // Advanced Implementation - Not in place yet
    err_code = adv_tx_pwr_char_add(p_ecs, p_ecs_init);
    VERIFY_SUCCESS(err_code);

    err_code = lock_state_char_add(p_ecs, p_ecs_init);
    VERIFY_SUCCESS(err_code);

    err_code = unlock_char_add(p_ecs, p_ecs_init);
    VERIFY_SUCCESS(err_code);

    err_code = pub_ecdh_key_char_add(p_ecs, p_ecs_init);
    VERIFY_SUCCESS(err_code);

    err_code = eid_id_key_char_add(p_ecs, p_ecs_init);
    VERIFY_SUCCESS(err_code);

    err_code = rw_adv_slot_char_add(p_ecs, p_ecs_init);
    VERIFY_SUCCESS(err_code);

    // Advanced Implementation - Not in place yet
    err_code = factory_reset_char_add(p_ecs, p_ecs_init);
    VERIFY_SUCCESS(err_code);

    // Advanced Implementation - Not in place yet
    err_code = remain_cnntbl_char_add(p_ecs, p_ecs_init);
    VERIFY_SUCCESS(err_code);

    return NRF_SUCCESS;
}
