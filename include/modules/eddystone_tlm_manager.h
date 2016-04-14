#ifndef EDDYSTONE_TLM_MANAGER_H
#define EDDYSTONE_TLM_MANAGER_H

#include "eddystone.h"

/**@brief Function for initializing the TLM manager
 * @retval see @ref app_timer_start
 */
ret_code_t eddystone_tlm_manager_init(void);

/**@brief Function for getting the current TLM
 * @param[in] p_tlm_frame   pointer to the tlm frame to which the frame will be retrieved
 */
void eddystone_tlm_manager_tlm_get(eddystone_tlm_frame_t * p_tlm_frame);

/**@brief Function for getting the current eTLM
 * @param[in] eik_pair_slot  the slot index of the EID (containing an EIK) to which the eTLM will be paired
 * @param[in] p_etlm_frame   pointer to the etlm frame to which the frame will be retrieved
 */
void eddystone_tlm_manager_etlm_get( uint8_t eik_pair_slot, eddystone_etlm_frame_t * p_etlm_frame);

/**@brief Function for increase ADV_CNT field of the TLM frame
 * @details should be called everytime a frame is advertised
 *
 * @param[in]  n    the number of frames to add to the count
 */
void eddystone_tlm_manager_adv_cnt_add( uint8_t n );

#endif /*EDDYSTONE_TLM_MANAGER_H*/
