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

#define NUM_OF_BLOCKS       APP_MAX_ADV_SLOTS + 4   /*see @eddystone_flash_init */

typedef PACKED(struct)
{
    uint8_t buffer[FLASH_BLOCK_SIZE];
} flash_buffers_t;

static flash_buffers_t m_flash_buffers[NUM_OF_BLOCKS] = {0}; //pstorage write requires static buffer

ret_code_t eddystone_flash_access_lock_key(uint8_t * p_lock_key, eddystone_flash_access_t access_type)
{
    ret_code_t err_code;
    pstorage_handle_t lock_key_handle;
    const uint8_t blk_index = APP_MAX_ADV_SLOTS+2;
    pstorage_block_identifier_get(&m_pstorage_base_handle, blk_index, &lock_key_handle);

    switch (access_type)
    {
        case EDDYSTONE_FLASH_ACCESS_READ:
            err_code = pstorage_load(p_lock_key,
                                     &lock_key_handle,
                                     ECS_AES_KEY_SIZE,
                                     0);
            RETURN_IF_ERROR(err_code);
            break;
        case EDDYSTONE_FLASH_ACCESS_WRITE:
            memcpy(m_flash_buffers[blk_index].buffer, p_lock_key, ECS_AES_KEY_SIZE);
            err_code = pstorage_update(&lock_key_handle,
                                       m_flash_buffers[blk_index].buffer,
                                       ECS_AES_KEY_SIZE,
                                       0);
            RETURN_IF_ERROR(err_code);
            break;
        case EDDYSTONE_FLASH_ACCESS_CLEAR:
            err_code = pstorage_clear(&lock_key_handle,
                                      ECS_AES_KEY_SIZE);
            RETURN_IF_ERROR(err_code);
            break;
        default:

            break;
    }
    return NRF_SUCCESS;
}

ret_code_t eddystone_flash_access_ecdh_key_pair(uint8_t * p_priv_key,
                                                uint8_t * p_pub_key,
                                                eddystone_flash_access_t access_type)
{
    ret_code_t err_code;
    pstorage_handle_t priv_ecdh_handle;
    pstorage_handle_t pub_ecdh_handle;

    const uint8_t blk_index_priv = APP_MAX_ADV_SLOTS;
    const uint8_t blk_index_pub = APP_MAX_ADV_SLOTS + 1;
    //Private key block is immediately after the last slot config, and public key is immediately after the privtae key

    pstorage_block_identifier_get(&m_pstorage_base_handle, blk_index_priv , &priv_ecdh_handle);
    pstorage_block_identifier_get(&m_pstorage_base_handle, blk_index_pub , &pub_ecdh_handle);

    switch (access_type)
    {
        case EDDYSTONE_FLASH_ACCESS_READ:
            err_code = pstorage_load(p_priv_key,
                                     &priv_ecdh_handle,
                                     FLASH_BLOCK_SIZE,
                                     0);
            RETURN_IF_ERROR(err_code);
            err_code = pstorage_load(p_pub_key,
                                     &pub_ecdh_handle,
                                     FLASH_BLOCK_SIZE,
                                     0);
            RETURN_IF_ERROR(err_code);
            break;
        case EDDYSTONE_FLASH_ACCESS_WRITE:
            memcpy(m_flash_buffers[blk_index_priv].buffer, p_priv_key, FLASH_BLOCK_SIZE);
            memcpy(m_flash_buffers[blk_index_pub].buffer, p_pub_key, FLASH_BLOCK_SIZE);

            err_code = pstorage_update(&priv_ecdh_handle,
                                       m_flash_buffers[blk_index_priv].buffer,
                                       FLASH_BLOCK_SIZE,
                                       0);
            RETURN_IF_ERROR(err_code);
            err_code = pstorage_update(&pub_ecdh_handle,
                                       m_flash_buffers[blk_index_pub].buffer,
                                       FLASH_BLOCK_SIZE,
                                       0);
            RETURN_IF_ERROR(err_code);
            break;
        case EDDYSTONE_FLASH_ACCESS_CLEAR:
            err_code = pstorage_clear(&priv_ecdh_handle,
                                      FLASH_BLOCK_SIZE);
            RETURN_IF_ERROR(err_code);
            err_code = pstorage_clear(&pub_ecdh_handle,
                                      FLASH_BLOCK_SIZE);
            RETURN_IF_ERROR(err_code);
            break;
        default:

            break;
    }
    return NRF_SUCCESS;
}

ret_code_t eddystone_flash_access_slot_configs(uint8_t slot_no,
                                               eddystone_flash_slot_config_t * p_config,
                                               eddystone_flash_access_t access_type)
{
    ret_code_t err_code;
    pstorage_handle_t slot_pstorage_handle;
    pstorage_block_identifier_get(&m_pstorage_base_handle, slot_no, &slot_pstorage_handle);

    switch (access_type)
    {
        case EDDYSTONE_FLASH_ACCESS_READ:
            err_code = pstorage_load((uint8_t*)p_config,
                                     &slot_pstorage_handle,
                                     FLASH_BLOCK_SIZE,
                                     0);
            RETURN_IF_ERROR(err_code);
            break;
        case EDDYSTONE_FLASH_ACCESS_WRITE:
            memcpy(m_flash_buffers[slot_no].buffer, p_config, FLASH_BLOCK_SIZE);
            err_code = pstorage_update(&slot_pstorage_handle,
                                       m_flash_buffers[slot_no].buffer,
                                       FLASH_BLOCK_SIZE,
                                       0);
            RETURN_IF_ERROR(err_code);
            break;
        case EDDYSTONE_FLASH_ACCESS_CLEAR:
            err_code = pstorage_clear(&slot_pstorage_handle,
                                      FLASH_BLOCK_SIZE);
            RETURN_IF_ERROR(err_code);
            break;
        default:
            break;
    }
    return NRF_SUCCESS;
}

ret_code_t eddystone_flash_access_flags(eddystone_flash_flags_t * p_flags, eddystone_flash_access_t access_type)
{
    ret_code_t err_code;
    pstorage_handle_t flags_handle;
    const uint8_t blk_index = APP_MAX_ADV_SLOTS+3;
    pstorage_block_identifier_get(&m_pstorage_base_handle, blk_index, &flags_handle);

    switch (access_type)
    {
        case EDDYSTONE_FLASH_ACCESS_READ:
            err_code = pstorage_load((uint8_t*)p_flags,
                                     &flags_handle,
                                     FLASH_BLOCK_SIZE,
                                     0);
            RETURN_IF_ERROR(err_code);
            break;
        case EDDYSTONE_FLASH_ACCESS_WRITE:
            memcpy(m_flash_buffers[blk_index].buffer, p_flags, FLASH_BLOCK_SIZE);
            err_code = pstorage_update(&flags_handle,
                                       m_flash_buffers[blk_index].buffer,
                                       FLASH_BLOCK_SIZE,
                                       0);
            RETURN_IF_ERROR(err_code);
            break;
        case EDDYSTONE_FLASH_ACCESS_CLEAR:
            err_code = pstorage_clear(&flags_handle,
                                      FLASH_BLOCK_SIZE);
            RETURN_IF_ERROR(err_code);
            break;
        default:

            break;
    }
    return NRF_SUCCESS;
}

uint32_t eddystone_flash_num_pending_ops(void)
{
    ret_code_t err_code;
    uint32_t num_pending;
    err_code = pstorage_access_status_get(&num_pending);
    APP_ERROR_CHECK(err_code);
    return num_pending;
}

bool eddystone_flash_read_is_empty(uint8_t * p_input_array, uint8_t length)
{
    uint8_t empty_check[FLASH_BLOCK_SIZE] = {0};
    memset(empty_check, 0xFF, sizeof(empty_check));
    if (memcmp(empty_check, p_input_array, length) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

ret_code_t eddystone_flash_init(pstorage_ntf_cb_t ps_cb)
{
    ret_code_t                err_code;
    pstorage_module_param_t pstorage_params;

    pstorage_init();

    pstorage_params.cb          = ps_cb;
    pstorage_params.block_size  = FLASH_BLOCK_SIZE;
    pstorage_params.block_count = NUM_OF_BLOCKS;
    //One block for each slot's config, 2 for ECDH pair, 1 for lock key, 1 for Factory state flag

    /* Flash Block Layout:
    [ Slot 0 config ].... [ Slot (APP_MAX_ADV_SLOTS - 1) config] [ Private ECDH ] [ Public ECDH ] [Lock Key] [Flags]
    */
    err_code = pstorage_register(&pstorage_params, &m_pstorage_base_handle);
    RETURN_IF_ERROR(err_code);

    #ifdef ERASE_FLASH_ON_REBOOT
    DEBUG_PRINTF(0, "Clearing all configurations stored in \r\n",0);
    err_code = pstorage_clear(&m_pstorage_base_handle, pstorage_params.block_size*pstorage_params.block_count);
    APP_ERROR_CHECK(err_code);

    uint32_t ops_pending = eddystone_flash_num_pending_ops();
    while (ops_pending != 0)
    {
      ops_pending = eddystone_flash_num_pending_ops();
    }
    #endif

    return NRF_SUCCESS;
}
