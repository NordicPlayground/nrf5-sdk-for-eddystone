#ifndef EDDYSTONE_FLASH_H
#define EDDYSTONE_FLASH_H

#include <stdint.h>
#include <stdbool.h>

#define FLASH_BLOCK_SIZE    ECS_AES_KEY_SIZE

/**@brief Non-voltaile key types.*/
typedef enum
{
    EDDYSTONE_FLASH_DATA_TYPE_LK, //lock key/code
    EDDYSTONE_FLASH_DATA_TYPE_IK  //identity key
} eddystone_flash_data_type_t;

/**@brief Function for storing keys to flash
 *
 * @param[in]   slot_no     Only used when type is EDDYSTONE_FLASH_DATA_TYPE_IK
 * @param[in]   type        LK or IK
 * @param[in]   p_data      pointer to the key to write
 * @retval      see @ref pstorage_update
 */
uint32_t eddystone_key_flash_store(uint8_t slot_no, eddystone_flash_data_type_t type, uint8_t * p_data);

/**@brief Function for loading keys to flash
 *
 * @param[in]   slot_no     Only used when type is EDDYSTONE_FLASH_DATA_TYPE_IK
 * @param[in]   type        LK or IK
 * @param[out]  p_data      pointer to the read buffer
 * @retval      see @ref pstorage_load
 */
uint32_t eddystone_key_flash_load(uint8_t slot_no, eddystone_flash_data_type_t type, uint8_t * p_data);

/**@brief Function for clearing keys from flash
 *
 * @param[in]   slot_no   Only used when type is EDDYSTONE_FLASH_DATA_TYPE_IK
 * @retval      see @ref pstorage_clear
 */
uint32_t eddystone_key_flash_clear(uint8_t slot_no);

/**@brief Function for clearing keys from flash
 *
 * @param[in]   slot_no  the slot index of the K scaler to store
 * @param[in]   scaler      the value of the K scaler
 * @retval      see @ref pstorage_update
 */
uint32_t eddystone_scaler_flash_store(uint8_t slot_no, uint8_t scaler);

/**@brief Function for loading all K rotation scalers from flash
 *
 * @param[out]  p_scalers_buffer   pointer to the read buffer of minimum FLASH_BLOCK_SIZE
 * @retval      see @ref pstorage_load
 */
uint32_t eddystone_scalers_flash_load(uint8_t * p_scalers_buffer);

/**@brief Function for checking if the flash is busy*/
bool eddystone_flash_is_busy(void);

/**@brief Function for initializing the flash module
 * @details If a key already exists in flash, then this function will copy the existing key into p_lock_key
 *
 * @param[in, out] p_lock_key   pointer to the 16 byte key to store into/ retrieve from flash
 */
uint32_t eddystone_flash_init(uint8_t * p_lock_key);


#endif /*EDDYSTONE_FLASH_H*/
