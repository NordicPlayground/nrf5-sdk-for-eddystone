#include "eddystone_flash.h"
#include "pstorage.h"
#include "pstorage_platform.h"
#include "macros_common.h"
#include "ecs_defs.h"
#include <string.h>
#include "eddystone_app_config.h"
#include "debug_config.h"

static pstorage_handle_t m_pstorage_base_handle;

#ifdef FLASH_DEBUG
    #include "SEGGER_RTT.h"
    #define DEBUG_PRINTF SEGGER_RTT_printf
#else
    #define DEBUG_PRINTF(...)
#endif

#define FLASH_OP_WAIT()     while (!m_flash_wait_flag_cleared){}

static volatile bool m_flash_wait_flag_cleared = true;

static uint8_t m_flash_scaler_buff[FLASH_BLOCK_SIZE] = {0};


/**@brief Function for handling pstorage events. Clears the flash wait flag */
static void eddystone_pstorage_handler( pstorage_handle_t * p_handle,
                                        uint8_t             op_code,
                                        uint32_t            result,
                                        uint8_t *           p_data,
                                        uint32_t            data_len )
{
        if (result == NRF_SUCCESS)
        {
            m_flash_wait_flag_cleared = true;
            switch (op_code)
            {
                case PSTORAGE_STORE_OP_CODE:
                    DEBUG_PRINTF(0," Flash Store Sucess \r\n", 0);
                    break;
                case PSTORAGE_LOAD_OP_CODE:
                    DEBUG_PRINTF(0," Flash Load Sucess \r\n", 0);
                    break;
                case PSTORAGE_CLEAR_OP_CODE:
                    DEBUG_PRINTF(0," Flash Clear Sucess \r\n", 0);
                    break;
                case PSTORAGE_UPDATE_OP_CODE:
                    DEBUG_PRINTF(0," Flash Update Sucess \r\n", 0);
                    break;
                default:
                    break;
            }
        }
        else
        {
            DEBUG_PRINTF(0," Flash Fail \r\n", 0);
            APP_ERROR_CHECK(result);
        }
}

uint32_t eddystone_scaler_flash_store(uint8_t slot_no, uint8_t scaler)
{
    uint32_t            err_code;
    pstorage_handle_t   k_scaler_flash_handle;
    uint8_t             scaler_block_offset = APP_MAX_ADV_SLOTS + 1;
    pstorage_block_identifier_get(&m_pstorage_base_handle, scaler_block_offset, &k_scaler_flash_handle);

    memset(m_flash_scaler_buff + slot_no, scaler, 1);

    DEBUG_PRINTF(0, "Storing K Scaler Slot: [%d] to NVM\r\n", slot_no);

    m_flash_wait_flag_cleared = false;
    err_code = pstorage_update(&k_scaler_flash_handle,
                              m_flash_scaler_buff,
                              FLASH_BLOCK_SIZE,
                              0);
    RETURN_IF_ERROR(err_code);

    return NRF_SUCCESS;
}

uint32_t eddystone_scalers_flash_load( uint8_t * p_scalers_buff )
{
    uint32_t            err_code;
    pstorage_handle_t   k_scaler_flash_handle;
    uint8_t             scaler_block_offset = APP_MAX_ADV_SLOTS + 1;
    pstorage_block_identifier_get(&m_pstorage_base_handle, scaler_block_offset, &k_scaler_flash_handle);

    m_flash_wait_flag_cleared = false;
    err_code = pstorage_load( m_flash_scaler_buff,
                              &k_scaler_flash_handle,
                              FLASH_BLOCK_SIZE,
                              0);
    RETURN_IF_ERROR(err_code);

    FLASH_OP_WAIT();

    DEBUG_PRINTF(0, "Loaded K Scalers from NVM:", 0);
    for (uint8_t i = 0; i < FLASH_BLOCK_SIZE; i++)
    {
        DEBUG_PRINTF(0,"0x%02x, ", m_flash_scaler_buff[i]);
    }
    DEBUG_PRINTF(0,"\r\n",0);

    memcpy(p_scalers_buff, m_flash_scaler_buff, FLASH_BLOCK_SIZE);

    return NRF_SUCCESS;
}

uint32_t eddystone_key_flash_store(uint8_t slot_no, eddystone_flash_data_type_t type, uint8_t * p_data)
{
    uint32_t err_code;
    pstorage_handle_t identity_key_flash_handle;
    pstorage_block_identifier_get(&m_pstorage_base_handle, slot_no + 1, &identity_key_flash_handle);

    switch (type)
    {
        case EDDYSTONE_FLASH_DATA_TYPE_LK:
            DEBUG_PRINTF(0, "Storing Lock Code to NVM\r\n",0);
            m_flash_wait_flag_cleared = false;

            err_code = pstorage_update(&m_pstorage_base_handle,
                                      p_data,
                                      ECS_AES_KEY_SIZE,
                                      0);
            RETURN_IF_ERROR(err_code);
            break;
        case EDDYSTONE_FLASH_DATA_TYPE_IK:
            DEBUG_PRINTF(0, "Storing Identity Key to NVM\r\n",0);
            m_flash_wait_flag_cleared = false;

            err_code = pstorage_update(&identity_key_flash_handle,
                                      p_data,
                                      ECS_AES_KEY_SIZE,
                                      0);
            RETURN_IF_ERROR(err_code);
            break;
        default:
            break;
    }
    return NRF_SUCCESS;

}

uint32_t eddystone_key_flash_load(uint8_t slot_no, eddystone_flash_data_type_t type, uint8_t * p_data)
{
    uint32_t                err_code;
    pstorage_handle_t identity_key_flash_handle;
    pstorage_block_identifier_get(&m_pstorage_base_handle, slot_no + 1, &identity_key_flash_handle);

    m_flash_wait_flag_cleared = false;

    switch (type) {
        case EDDYSTONE_FLASH_DATA_TYPE_LK:
            err_code = pstorage_load(p_data,
                                 &m_pstorage_base_handle,
                                 ECS_AES_KEY_SIZE,
                                 0);
            RETURN_IF_ERROR(err_code);
            break;
        case EDDYSTONE_FLASH_DATA_TYPE_IK:
            err_code = pstorage_load(p_data,
                                 &identity_key_flash_handle,
                                 ECS_AES_KEY_SIZE,
                                 0);
            RETURN_IF_ERROR(err_code);
            break;
    }

    return NRF_SUCCESS;
}

uint32_t eddystone_key_flash_clear(uint8_t slot_no)
{
    uint32_t                err_code;
    pstorage_handle_t identity_key_flash_handle;
    pstorage_block_identifier_get(&m_pstorage_base_handle, slot_no + 1, &identity_key_flash_handle);

    m_flash_wait_flag_cleared = false;

    err_code = pstorage_clear(&identity_key_flash_handle, ECS_AES_KEY_SIZE);
    RETURN_IF_ERROR(err_code);

    return NRF_SUCCESS;
}

bool eddystone_flash_is_busy(void)
{
    return !m_flash_wait_flag_cleared;
}

uint32_t eddystone_flash_init(uint8_t * p_lock_key)
{
    uint32_t                err_code;
    pstorage_module_param_t pstorage_params;
    eddystone_flash_data_type_t lock_code_type = EDDYSTONE_FLASH_DATA_TYPE_LK;
    uint8_t flash_empty[ECS_AES_KEY_SIZE] = {0x00};
    uint8_t key_in_flash[ECS_AES_KEY_SIZE] = {0x00};

    memset(flash_empty, 0xFF, ECS_AES_KEY_SIZE);

    pstorage_init();

    pstorage_params.cb          = eddystone_pstorage_handler;
    pstorage_params.block_size  = FLASH_BLOCK_SIZE;
    pstorage_params.block_count = APP_MAX_ADV_SLOTS + 2; //one block for each slot's IK, 1 for global LK, 1 for all the scaling factors

    /**For convenience of flash access (block size must be multiples of 4 bytes) and
       high frequency of key storage, having a FLASH_BLOCK_SIZE of ECS_AES_KEY_SIZE (16 bytes)
       also means that there can only be 16 K scalers stored in one block. Thus under the current flash
       implementation, the absolute maximum number of EID slots supported is 16. Albeit
       it is highly unlikely there will ever be a need for anywhere close to 16 EID slots in one beacon
    */

    /* Flash Block Layout:
    [ LOCK KEY ] [ IK Slot 0] ....... [ IK slot (APP_MAX_ADV_SLOTS - 1)] [ Scalers slot 0 ... (APP_MAX_ADV_SLOTS - 1)]
    */

    err_code = pstorage_register(&pstorage_params, &m_pstorage_base_handle);
    RETURN_IF_ERROR(err_code);

    #ifdef ERASE_FLASH_ON_REBOOT
    DEBUG_PRINTF(0, "Clearing ALL registered NVM\r\n",0);
    m_flash_wait_flag_cleared = false;
    err_code = pstorage_clear(&m_pstorage_base_handle, pstorage_params.block_size*pstorage_params.block_count);
    APP_ERROR_CHECK(err_code);
    FLASH_OP_WAIT();
    #endif

    //Read the lock key
    err_code = eddystone_key_flash_load(0xFF, lock_code_type, key_in_flash);
    RETURN_IF_ERROR(err_code);

    FLASH_OP_WAIT();

    DEBUG_PRINTF(0,"Lock Key Read From Flash:",0);
    for (uint8_t i = 0; i < 16; i++)
    {
        DEBUG_PRINTF(0,"0x%02x, ", key_in_flash[i]);
    }
    DEBUG_PRINTF(0,"\r\n",0);

    //If flash is empty, write new key to it, otherwise return the key from flash
    if(memcmp(flash_empty,key_in_flash,ECS_AES_KEY_SIZE) == 0)
    {
        eddystone_key_flash_store(0, lock_code_type, p_lock_key);
        FLASH_OP_WAIT();
    }
    else
    {
        memcpy(p_lock_key, key_in_flash, ECS_AES_KEY_SIZE);
    }

    return NRF_SUCCESS;
}
