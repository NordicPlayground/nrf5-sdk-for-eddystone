#ifndef EDDYSTONE_SECURITY_H__
#define EDDYSTONE_SECURITY_H__

#include "ble_ecs.h"
#include "app_error.h"


typedef enum
{
    EDDYSTONE_SECURITY_MSG_UNLOCKED,        /**< Beacon is Unlocked*/
    EDDYSTONE_SECURITY_MSG_EID,             /**< EID has been generated*/
    EDDYSTONE_SECURITY_MSG_IK,              /**< IK has been generated */
    EDDYSTONE_SECURITY_MSG_ECDH,            /**< Public ECDH has been generated*/
    EDDYSTONE_SECURITY_MSG_STORE_TIME       /**< EID slot time needs to be stored*/
} eddystone_security_msg_t;

typedef void (*eddystone_security_msg_cb_t)(uint8_t slot_no,
                                            eddystone_security_msg_t msg_type);
typedef struct
{
    eddystone_security_msg_cb_t    msg_cb;  /**< Callback function pointer used by the security module to pass out events*/
} eddystone_security_init_t;

/**@brief structure used to preserve/restore an EID slot*/
typedef PACKED(struct)
{
    eddystone_frame_type_t frame_type;
    uint8_t                k_scaler;
    uint32_t               seconds;
    uint8_t                ik[ECS_AES_KEY_SIZE];
} eddystone_eid_config_t;

typedef ble_ecs_lock_state_read_t eddystone_security_lock_state_t;

/**@brief Initialize the security module
 * @param[in] p_cb_init       pointer to the security init struct
 * @retval see @ref app_timer_start
 */
ret_code_t eddystone_security_init (eddystone_security_init_t * p_cb_init);

/**@brief Updates the new lock code and puts it into flash
 * @param[in] p_ecrypted_key       pointer to new lock code
 * @retval see @ref eddystone_flash_ik_store
 */
ret_code_t eddystone_security_lock_code_update( uint8_t * p_ecrypted_key );

/**@brief Reads the challenge and encrypts it with AES_ECB
 * @detail the result of the encryption is compared with the provided unlock token
 *         in eddystone_security_unlock_verify()
 * @param[in] p_challenge       pointer to the challenge buffer
 * @retval see @ref sd_ecb_block_encrypt
 */
ret_code_t eddystone_security_unlock_prepare( uint8_t * p_challenge );

/**@brief Compare the result from unlock_prepare to the input unlock_token and unlocks the beacon if matching
 * @param[in] p_unlock_token    the unlock token written by the client
 */
void eddystone_security_unlock_verify( uint8_t * p_unlock_token );

/**@brief Generates a random challenge for the unlock characteristic
 * param[out]   p_rand_chlg_buff    pointer to buffer where to random challenge will be copied to
 * @retval      see @ref sd_rand_application_vector_get
 */
ret_code_t eddystone_security_random_challenge_generate( uint8_t * p_rand_chlg_buff );

/**@brief Stores the Public ECDH key from the client in the beacon registration process.
* @details This function will begin a series of crypto activities including the generation of temporary keys and EIDs
*
* @param[in] slot_no         the index of the slot whose public ECDH key will be retrieved
* @param[in] p_pub_ecdh      pointer to the public ECDH
* @param[in] scaler_k        K rotation scaler
*/
ret_code_t eddystone_security_client_pub_ecdh_receive( uint8_t slot_no, uint8_t * p_pub_ecdh, uint8_t scaler_k );
/**@brief Stores the shared IK from the client in the beacon registration process.
* @details This function will begin a series of crypto activities including the generation of temporary keys and EIDs
*
* @param[in] slot_no         the index of the slot whose public ECDH key will be retrieved
* @param[in] p_encrypted_ik  pointer to the received IK
* @param[in] scaler_k        K rotation scaler
*/
ret_code_t eddystone_security_shared_ik_receive( uint8_t slot_no, uint8_t * p_encrypted_ik, uint8_t scaler_k );

/**@brief Copies the 32-byte ECDH key into the buffer provided
* @param[in]  slot_no         the index of the slot whose public ECDH key will be retrieved
* @param[out] p_edch_buffer   pointer to the buffer
*/
void eddystone_security_pub_ecdh_get(uint8_t slot_no, uint8_t * p_edch_buffer );

/**@brief Returns the beacon clock value in (LITTLE ENDIAN)
 * @param[in] slot_no        the index of the slot
 * @retval    32-bit clock value
 */
uint32_t eddystone_security_clock_get( uint8_t slot_no );

/**@brief Returns the rotation exponent scaler value
 * @param[in] slot_no        the index of the slot
 * @retval    K rotation scaler
 */
uint8_t eddystone_security_scaler_get( uint8_t slot_no );

/**@brief Copies the 8-byte EID into the buffer provided
* @param[in] slot_no        the index of the slot whose EID will be retrieved
* @param[out] p_eid_buffer  pointer to the buffer
*/
void eddystone_security_eid_get( uint8_t slot_no, uint8_t * p_eid_buffer );

/**@brief Function to restore EID slot
* @param[in] slot_no        the index of the slot to restore
* @param[in] p_restore_data  pointer to the restore data structure
*/
void eddystone_security_eid_slots_restore( uint8_t slot_no, eddystone_eid_config_t * p_restore_data );

/**@brief Destroy stored EID states - should be called when the slot if overwritten as another slot, or cleared by empty byte/single 0
 * @param[in] slot_no  the index of the slot to destroy
 */
void eddystone_security_eid_slot_destroy( uint8_t slot_no );

/**@brief Function for fetching the EID config */
void eddystone_security_eid_config_get( uint8_t slot_no, eddystone_eid_config_t * p_config);

/**@brief Preserve ECDH key pair by writing to flash
 * @retval see @ref eddystone_flash_access_ecdh_key_pair
 */
ret_code_t eddystone_security_ecdh_pair_preserve( void );

/**@brief Copies the 16-byte EID ID Key into the buffer provided
 * @param[in]   slot_no         slot index of the EID slot whose IK will be retrieved
 * @param[out]  p_key_buffer    buffer for the key
 */
void eddystone_security_plain_eid_id_key_get( uint8_t slot_no, uint8_t * p_key_buffer );

/**@brief Copies the 16-byte LK encrypted EID ID Key into the buffer provided
 * @param[in]   slot_no         slot index of the EID slot whose encrypted IK will be retrieved
 * @param[out]  p_key_buffer    buffer for the key
 */
void eddystone_security_encrypted_eid_id_key_get( uint8_t slot_no, uint8_t * p_key_buffer );

/**@brief Converts a TLM frame into an eTLM frame using the EIK of the ik_slot_no'th slot
 * @param[in]   ik_slot_no  slot index of the EID slot whose IK will be paired with the eTLM
 * @param[in]   p_tlm       pointer to the TLM frame buffer
 * @param[out]  p_etlm      pointer to the eTLM frame buffer
 */
void eddystone_security_tlm_to_etlm( uint8_t ik_slot_no, eddystone_tlm_frame_t * p_tlm, eddystone_etlm_frame_t * p_etlm );

#endif  /*EDDYSTONE_SECURITY_H__*/
