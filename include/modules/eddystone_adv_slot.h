#ifndef EDDYSTONE_ADV_SLOT_H
#define EDDYSTONE_ADV_SLOT_H

#include <stdint.h>
#include "ble_ecs.h"
#include "eddystone.h"

#define SLOT_BOUNDARY_CHECK(slot)               \
        if(slot > (APP_MAX_ADV_SLOTS - 1))      \
        {                                       \
            slot = (APP_MAX_ADV_SLOTS - 1);     \
        }                                       \
        else                                    \
        {                                       \
            slot = slot;                        \
        }                                       \

/**@brief Union containing all the advertisable frame types that can be easily
passed in to advdata when slot advertising */
typedef union
{
    eddystone_uid_frame_t   uid;
    eddystone_url_frame_t   url;
    eddystone_tlm_frame_t   tlm;
    eddystone_eid_frame_t   eid;
    eddystone_etlm_frame_t  etlm;
} eddystone_adv_frame_t;

/**@brief Structure that directly interfaces with the R/W ADV slot operations*/
typedef struct
{
    uint8_t                 slot_no;                                                /** identifier for the slot, indexed at 0 */
    ble_ecs_adv_intrvl_t    adv_intrvl;                                             /** advertising interval in ms */
    ble_ecs_radio_tx_pwr_t  radio_tx_pwr;                                           /** radio tx pwr in dB*/
    int8_t                  frame_write_buffer[ECS_ADV_SLOT_CHAR_LENGTH_MAX];       /** RW frame data for the slot that come from the Central*/
    uint16_t                frame_write_length;                                     /** Length of the frame_write_buffer that is occupied with data */
    ble_ecs_eid_id_key_t    encrypted_eid_id_key;                                   /** EID key for the slot*/
    eddystone_adv_frame_t   adv_frame;                                              /** Frame structure to be passed in for advertising data */
} eddystone_adv_slot_t;


/**@brief Structure needed by advertising manager to advertise the frame*/
typedef struct
{
    ble_ecs_adv_intrvl_t        adv_intrvl;
    ble_ecs_radio_tx_pwr_t      radio_tx_pwr;
    eddystone_frame_type_t      frame_type;
    eddystone_adv_frame_t       * p_adv_frame;
    uint8_t                     url_frame_length;
} eddystone_adv_slot_params_t;

/**@brief Function to initialize the eddystone advertising slots with default values
 *
 * @details This function will synchronize ALL the slots with the initial values of the relevant characteristics:
 *          Advertising interval, TX power, R/W ADV Slot etc.
 *
 * @param[in]   p_ble_ecs_init   Pointer to the ECS init struct
 */
void eddystone_adv_slots_init( ble_ecs_init_t * p_ble_ecs_init );

/** @note For the setter and getter functions, if the slot_no is larger than maximum allowable value
 *       (defined in broadcast capabilities characteristic), then the highest slot will be written to.
 * @note if the provided adv interval or tx power is not reasonable (outside of BLE spec), then
 *       the pointer will be quitely modified to a reasonable value and which will be used to write to the slot
 *       and should also be used to write to the advertising interval characteristic
 */

/**@brief Function for setting the advertising interval (is ms) of the slot_no'th slot
 *
 * @warning For compatibility with eddystone specifications, p_adv_intrvl must point to
 *          a 16-bit BIG ENDIAN value (coming from the characteristic write request)
 *          which is then converted to a small endian value inside the function before written into the vairable in the slot
 *
 * @param[in]       slot_no         the slot index
 * @param[in,out]   p_adv_intrvl    pointer to the adv interval to set
 * @param[in]       global          Should be set if the beacon does not support variable advertising intervals
 */
void eddystone_adv_slot_adv_intrvl_set( uint8_t slot_no, ble_ecs_adv_intrvl_t * p_adv_intrvl, bool global);

/**@brief Function for getting the advertising interval (is ms) of the slot_no'th slot
 *
 * @warning For compatibility with eddystone specifications, p_adv_intrvl will point to a converted 16-bit BIG ENDIAN value.
 *
 * @param[in]       slot_no         the slot index
 * @param[out]      p_adv_intrvl    pointer where to fetched adv interval will be stored
 * @param[in]       global          Should be set if the beacon does not support variable advertising intervals
 */
void eddystone_adv_slot_adv_intrvl_get( uint8_t slot_no, ble_ecs_adv_intrvl_t * p_adv_intrvl );

/**@brief Function for setting the TX power of the slot_no'th slot
 *
 * @note if the slot_no is larger than maximum allowable value
 *       (defined in broadcast capabilities characteristic), then the highest slot will be written to.
 * @note if the provided TX power is not reasonable (outside of BLE spec or hardware spec), then
 *       the pointer will be quitely modified to a reasonable value and which will be used to write to the slot
 *       and should also be used to write to the TX power characteristic
 *
 * @param[in]       slot_no         the slot index
 * @param[in,out]   p_radio_tx_pwr  pointer to the tx power to set
 * @param[in]       global          Should be set if the beacon does not support variable advertising intervals
 */
void eddystone_adv_slot_radio_tx_pwr_set( uint8_t slot_no, ble_ecs_radio_tx_pwr_t * p_radio_tx_pwr, bool global);

/**@brief Function for getting the TX power of the slot_no'th slot
 *
 * @note if the slot_no is larger than maximum allowable value
 *       (defined in broadcast capabilities characteristic), then the highest slot will be written to.
 *
 * @param[in]       slot_no         the slot index
 * @param[in,out]   p_radio_tx_pwr  pointer to the tx power to get
 *
 */
void eddystone_adv_slot_radio_tx_pwr_get( uint8_t slot_no, ble_ecs_radio_tx_pwr_t * p_radio_tx_pwr );

/**@brief Function for setting the R/W ADV of the slot_no'th slot
 *
 * @note if the slot_no is larger than maximum allowable value
 *       (defined in broadcast capabilities characteristic), then the highest slot will be written to.
 *
 * @param[in]       slot_no         the slot index
 * @param[in,out]   p_frame_data    pointer to a ble_ecs_rw_adv_slot_t where the data will be written from
 *
 */
void eddystone_adv_slot_rw_adv_data_set( uint8_t slot_no, ble_ecs_rw_adv_slot_t * p_frame_data );
/**@brief Function for getting the R/W ADV of the slot_no'th slot
 *
 * @note if the slot_no is larger than maximum allowable value
 *       (defined in broadcast capabilities characteristic), then the highest slot will be written to.
 *
 * @param[in]       slot_no         the slot index
 * @param[in,out]   p_frame_data    pointer to a ble_ecs_rw_adv_slot_t where the data will be retrieved to
 *
 */
void eddystone_adv_slot_rw_adv_data_get( uint8_t slot_no, ble_ecs_rw_adv_slot_t * p_frame_data );

/**@brief Function for writing the slot configuration to flash
*/
void eddystone_adv_slot_write_to_flash( uint8_t slot_no );

/**@brief Function for setting the slot's encrypted EID Identity Key
*
* @param[in]       slot_no         the slot index
* @param[in,out]   p_eid_id_key    pointer to a ble_ecs_eid_id_key_t where the key will be written from
*/
void eddystone_adv_slot_encrypted_eid_id_key_set( uint8_t slot_no, ble_ecs_eid_id_key_t * p_eid_id_key );

/**@brief Function for getting the slot's encrypted EID Identity Key
*
* @param[in]       slot_no         the slot index
* @param[in,out]   p_eid_id_key    pointer to a ble_ecs_eid_id_key_t where the key will be retrieved to
*/
ret_code_t eddystone_adv_slot_encrypted_eid_id_key_get( uint8_t slot_no, ble_ecs_eid_id_key_t * p_eid_id_key );

/**@brief Function for populating the advertising frame with the EID*/
void eddystone_adv_slot_eid_set( uint8_t slot_no );

/**@brief Function for getting the id and total number of slots that are EIDs
*
* @param[out]       p_which_slots_are_eids   optional: (pass in NULL to not us this feature)
*                                            pointer to a buffer at least APP_MAX_EID_SLOTS in length,
*                                            the buffer will be filled with the no. of the slots that are
                                             configured as EIDs in increasing order, 0xFF represent blanks
                                             (e.g. [0,2,5, 0xFF, 0xFF, 0xFF...] means slots 0,2,5 are EID slots)
* @retval          the total number of slots that are currently configured as EIDs
*/
uint8_t eddystone_adv_slot_num_of_current_eids(uint8_t * p_which_slots_are_eids);

/**@brief Similar to @ref eddystone_adv_slot_num_of_current_eids */
uint8_t eddystone_adv_slot_num_of_configured_slots(uint8_t * p_which_slots_are_configured);

/**@brief Whether or not the slot if the slot has been configured
* @param[in]       slot_no         the slot index
*/
bool eddystone_adv_slot_is_configured (uint8_t slot_no);

/**@brief Function for getting the parameters required by the advertising module to broadcast the slot
* @param[in]       slot_no         the slot index
* @param[in]       p_params        pointer to a eddystone_adv_slot_params_t where the data will be retrieved to
*/
void eddystone_adv_slot_params_get( uint8_t slot_no, eddystone_adv_slot_params_t * p_params);

#endif /*EDDYSTONE_ADV_SLOT_H*/
