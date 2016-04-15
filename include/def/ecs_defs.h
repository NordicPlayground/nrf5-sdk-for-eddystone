#ifndef ECS_DEFS_H__
#define ECS_DEFS_H__

#include "eddystone.h"

#define ECS_UID_READ_LENGTH                     (EDDYSTONE_UID_LENGTH)
#define ECS_UID_WRITE_LENGTH                    (EDDYSTONE_UID_NAMESPACE_LENGTH + EDDYSTONE_UID_INSTANCE_LENGTH + EDDYSTONE_FRAME_TYPE_LENGTH)


#define ECS_URL_READ_LENGTH                     (EDDYSTONE_URL_LENGTH)
#define ECS_URL_WRITE_LENGTH                    (EDDYSTONE_URL_URL_SCHEME_LENGTH + EDDYSTONE_URL_ENCODED_URL_LENGTH + EDDYSTONE_FRAME_TYPE_LENGTH)


#define ECS_TLM_READ_LENGTH                     (ECS_TLM_READ_LENGTH)
#define ECS_TLM_WRITE_LENGTH                    (EDDYSTONE_FRAME_TYPE_LENGTH)

#define ECS_EID_READ_LENGTH                     (14)
#define ECS_EID_WRITE_ECDH_LENGTH               (34)
#define ECS_EID_WRITE_IDK_LENGTH                (18)

//Defined in nRF52 Specifications
#define ECS_NUM_OF_SUPORTED_TX_POWER        (9)
#define ECS_SUPPORTED_TX_POWER              {0x04, 0x03, 0x00, 0xFC, 0xF8, 0xF4, 0xF0, 0xEC, 0xD8}
#define ECS_CALIBRATED_RANGING_DATA         {0xFB, 0xF9, 0xF7, 0xF2, 0xED, 0xE8, 0xE3, 0xD9, 0xCF}

//Defined in Eddystone Specifications
#define ECS_AES_KEY_SIZE                                  (16)
#define ECS_ECDH_KEY_SIZE                                 (32)

#define ECS_ADV_SLOT_CHAR_LENGTH_MAX                      (34) /*corresponds to when the slots is configured as an EID slot*/

/*Characteristic: Broadcast Capabilities*/

/* Field: ble_ecs_init_params_t.brdcst_cap.cap_bitfield*/
#define ECS_BRDCST_VAR_ADV_SUPPORTED_Yes                   (1)          /*set if the beacon supports individual per-slot adv intervals*/
#define ECS_BRDCST_VAR_ADV_SUPPORTED_No                    (0)
#define ECS_BRDCST_VAR_ADV_SUPPORTED_Pos                   (0)
#define ECS_BRDCST_VAR_ADV_SUPPORTED_Msk                   (1 << ECS_BRDCST_VAR_ADV_SUPPORTED_Pos)
#define ECS_BRDCST_VAR_TX_POWER_SUPPORTED_Yes              (1)          /*set if the beacon supports individual per-slot TX intervals*/
#define ECS_BRDCST_VAR_TX_POWER_SUPPORTED_No               (0)
#define ECS_BRDCST_VAR_TX_POWER_SUPPORTED_Pos              (1)
#define ECS_BRDCST_VAR_TX_POWER_SUPPORTED_Msk              (1 << ECS_BRDCST_VAR_TX_POWER_SUPPORTED_Pos)

#define ECS_BRDCST_VAR_RFU_MASK                            (0x03)       /*AND Mask to guarantee that bits 0x04 to 0x80 (RFU) are cleared */

/* Field: ble_ecs_init_params_t.brdcst_cap.supp_frame_types*/
#define ECS_FRAME_TYPE_UID_SUPPORTED_Yes                    (1)
#define ECS_FRAME_TYPE_UID_SUPPORTED_No                     (0)
#define ECS_FRAME_TYPE_UID_SUPPORTED_Pos                    (0)
#define ECS_FRAME_TYPE_UID_SUPPORTED_Msk                    (1 << ECS_FRAME_TYPE_UID_SUPPORTED_Pos)

#define ECS_FRAME_TYPE_URL_SUPPORTED_Yes                    (1)
#define ECS_FRAME_TYPE_URL_SUPPORTED_No                     (0)
#define ECS_FRAME_TYPE_URL_SUPPORTED_Pos                    (1)
#define ECS_FRAME_TYPE_URL_SUPPORTED_Msk                    (1 << ECS_FRAME_TYPE_URL_SUPPORTED_Pos)

#define ECS_FRAME_TYPE_TLM_SUPPORTED_Yes                    (1)
#define ECS_FRAME_TYPE_TLM_SUPPORTED_No                     (0)
#define ECS_FRAME_TYPE_TLM_SUPPORTED_Pos                    (2)
#define ECS_FRAME_TYPE_TLM_SUPPORTED_Msk                    (1 << ECS_FRAME_TYPE_TLM_SUPPORTED_Pos)

#define ECS_FRAME_TYPE_EID_SUPPORTED_Yes                    (1)
#define ECS_FRAME_TYPE_EID_SUPPORTED_No                     (0)
#define ECS_FRAME_TYPE_EID_SUPPORTED_Pos                    (3)
#define ECS_FRAME_TYPE_EID_SUPPORTED_Msk                    (1 << ECS_FRAME_TYPE_EID_SUPPORTED_Pos)

#define ECS_FRAME_TYPE_RFU_MASK                             (0x000F)        /*AND Mask to guarantee that bits 0x0010 to 0x8000 (RFU) are cleared */

/*Characteristic: Lock State: Lock State (READ)*/
#define ECS_LOCK_STATE_LOCKED                               (0x00)
#define ECS_LOCK_STATE_UNLOCKED                             (0x01)
#define ECS_LOCK_STATE_UNLOCKED_AUTO_RELOCK_DISABLED        (0x02)

/*Characteristic: Lock State: Lock Byte (WRITE)*/
#define ECS_LOCK_BYTE_LOCK                                  (0x00)
#define ECS_LOCK_BYTE_DISABLE_AUTO_RELOCK                   (0x02)


#endif /*BLE_ECS_DEFS_H__*/
