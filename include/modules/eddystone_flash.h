#ifndef EDDYSTONE_FLASH_H
#define EDDYSTONE_FLASH_H

#include <stdint.h>
#include <stdbool.h>
#include "ble_ecs.h"
#include "pstorage.h"
#include "eddystone_app_config.h"

#define FLASH_BLOCK_SIZE    32  //Minimum size 32, for ECDH key storage

#define FLASH_OP_WAIT()       uint32_t pending_ops = eddystone_flash_num_pending_ops(); \
                              while (pending_ops != 0)                                  \
                              {                                                         \
                                  pending_ops = eddystone_flash_num_pending_ops();      \
                              }                                                         \

/**@brief struct for writing and reading persistent slot config to/from flash
 * @note size is word aligned and also matches flash block size of 32 bytes
 */
typedef PACKED(struct)
{
    ble_ecs_adv_intrvl_t    adv_int;
    ble_ecs_radio_tx_pwr_t  radio_tx_pwr;
    ble_ecs_adv_tx_pwr_t    adv_tx_pwr;
    uint8_t                 frame_data[27];
    uint8_t                 data_length;
} eddystone_flash_slot_config_t;

typedef struct
{
    bool    factory_state;
    bool    slot_is_empty[APP_MAX_ADV_SLOTS];
    uint8_t rfu[FLASH_BLOCK_SIZE-APP_MAX_ADV_SLOTS-1];    //fill the unused bytes with rfu
} eddystone_flash_flags_t; //TODO: talk about having the flags struct match flash block size here

typedef enum
{
    EDDYSTONE_FLASH_ACCESS_READ,
    EDDYSTONE_FLASH_ACCESS_WRITE,
    EDDYSTONE_FLASH_ACCESS_CLEAR
} eddystone_flash_access_t;

/**@brief Function for accessing ECDH keys to/from flash
 *
 * @param[out,in]   p_priv_key     pointer to the private key r/w buffer
 * @param[out,in]   p_pub_key      pointer to the public key r/w buffer
 * @param[in]       access_type    see @eddystone_flash_access_t
 * @retval          see @ref pstorage_update, @ref pstorage_load, @ref pstorage_clear,
 */
 ret_code_t eddystone_flash_access_ecdh_key_pair(uint8_t * p_priv_key,
                                                 uint8_t * p_pub_key,
                                                 eddystone_flash_access_t access_type);

 /**@brief Function for accessing slot cnfigurations to/from flash
  *
  * @param[in]       slot_no        Slot index
  * @param[out,in]   p_config       pointer to the slot config r/w buffer
  * @param[in]       access_type    see @eddystone_flash_access_t
  * @retval          see @ref pstorage_update, @ref pstorage_load, @ref pstorage_clear,
  */
ret_code_t eddystone_flash_access_slot_configs(uint8_t slot_no,
                                               eddystone_flash_slot_config_t * p_config,
                                               eddystone_flash_access_t access_type);
/**@brief Function for accessing beacon lock key to/from flash
*
* @param[out,in]   p_lock_key     pointer to the lock key r/w buffer
* @param[in]       access_type    see @eddystone_flash_access_t
* @retval          see @ref pstorage_update, @ref pstorage_load, @ref pstorage_clear,
*/
ret_code_t eddystone_flash_access_lock_key(uint8_t * p_lock_key, eddystone_flash_access_t access_type);
/**@brief Function for accessing flash config flag from flash
*
* @param[out,in]   p_flags         pointer to the flag r/w buffer
* @param[in]       access_type    see @eddystone_flash_access_t
* @retval          see @ref pstorage_update, @ref pstorage_load, @ref pstorage_clear,
*/
ret_code_t eddystone_flash_access_flags(eddystone_flash_flags_t * p_flags, eddystone_flash_access_t access_type);
/**@brief Helper function to check if an array read from flash contains all 0xFFs
* @retval  true or false
*/
bool eddystone_flash_read_is_empty(uint8_t * p_input_array, uint8_t length);
/**@brief Function for retrieving the number of operations queued
* @retval  the number of operations queued
*/
uint32_t eddystone_flash_num_pending_ops(void);

/**@brief Function for initializing the flash module
 *@retval see @ref pstorage_register
 */
ret_code_t eddystone_flash_init(pstorage_ntf_cb_t ps_cb);


#endif /*EDDYSTONE_FLASH_H*/
