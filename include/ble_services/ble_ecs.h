#ifndef BLE_ECS_H__
#define BLE_ECS_H__

#include "ble.h"
#include "ble_srv_common.h"
#include "app_util_platform.h"
#include "sdk_common.h"
#include "ecs_defs.h"
#include <stdint.h>
#include <stdbool.h>

#define BLE_UUID_ECS_SERVICE                    0x7500

//Eddystone GATT characteristic data types for easy handling of char. data

/**@brief struct for data fields in Broadcast Capabilities characteristic*/
typedef PACKED(struct)
{
    int8_t     vers_byte;
    int8_t     max_supp_total_slots;
    int8_t     max_supp_eid_slots;
    int8_t     cap_bitfield;
    int16_t    supp_frame_types;
    int8_t     supp_radio_tx_power[ECS_NUM_OF_SUPORTED_TX_POWER];
} ble_ecs_brdcst_cap_t;

typedef uint8_t             ble_ecs_active_slot_t;
typedef uint16_t            ble_ecs_adv_intrvl_t;
typedef int8_t              ble_ecs_radio_tx_pwr_t;
typedef int8_t              ble_ecs_adv_tx_pwr_t;

/**@brief Enum for all read states of the Lock State characteristic*/
typedef enum
{
    BLE_ECS_LOCK_STATE_LOCKED                           = ECS_LOCK_STATE_LOCKED,
    BLE_ECS_LOCK_STATE_UNLOCKED                         = ECS_LOCK_STATE_UNLOCKED,
    BLE_ECS_LOCK_STATE_UNLOCKED_AUTO_RELOCK_DISABLED    = ECS_LOCK_STATE_UNLOCKED_AUTO_RELOCK_DISABLED
} ble_ecs_lock_state_read_t;

/**@brief Enum for all write bytes of the Lock State characteristic*/
typedef enum
{
    BLE_ECS_LOCK_BYTE_LOCK                  = ECS_LOCK_BYTE_LOCK,
    BLE_ECS_LOCK_BYTE_DISABLE_AUTO_RELOCK   = ECS_LOCK_BYTE_DISABLE_AUTO_RELOCK
} ble_ecs_lock_byte_t;

/**@brief Struct for the write data fields of the Lock State characteristic*/
typedef PACKED(struct)
{
    ble_ecs_lock_byte_t              lock_byte;
    int8_t                           encrypted_key[ECS_AES_KEY_SIZE];
} ble_ecs_lock_state_write_t;

/**@brief Union for the Lock State characteristic */
typedef union
{
    ble_ecs_lock_state_read_t   read;
    ble_ecs_lock_state_write_t  write;
} ble_ecs_lock_state_t;

/**@brief R/W Union for the Unlock characteristic */
typedef union
{
    int8_t     r_challenge[ECS_AES_KEY_SIZE];
    int8_t     w_unlock_token[ECS_AES_KEY_SIZE];
} ble_ecs_unlock_t;

/**@brief Struct for the Public ECDH Key characteristic*/
typedef PACKED(struct)
{
    int8_t     key[ECS_ECDH_KEY_SIZE];
} ble_ecs_public_ecdh_key_t;

/**@brief Struct for the EID Identity Key characteristic*/
typedef PACKED(struct)
{
    int8_t     key[ECS_AES_KEY_SIZE];
} ble_ecs_eid_id_key_t;

/**@brief Struct for the Read/Write ADV Slot characteristic
 * @details p_data is a pointer only to the section of the data AFTER the frame type
 * @details char_length is the length of the frame_type + the length of the data pointed to by p_data
 */
typedef PACKED(struct)
{
    eddystone_frame_type_t           frame_type;
    int8_t  *                        p_data;
    uint16_t                         char_length;
} ble_ecs_rw_adv_slot_t;

typedef uint8_t ble_ecs_factory_reset_t;

/**@brief R/W Union of Unlock characteristic */
typedef union
{
    uint8_t r_is_non_connectable_supported;
    uint8_t w_remain_connectable_boolean;
} ble_ecs_remain_conntbl_t;

/**@brief eddystone configuration service event types (corresponds to each char.) */
typedef enum
{
    BLE_ECS_EVT_BRDCST_CAP,
    BLE_ECS_EVT_ACTIVE_SLOT,
    BLE_ECS_EVT_ADV_INTRVL,
    BLE_ECS_EVT_RADIO_TX_PWR,
    BLE_ECS_EVT_ADV_TX_PWR,
    BLE_ECS_EVT_LOCK_STATE,
    BLE_ECS_EVT_UNLOCK,
    BLE_ECS_EVT_PUBLIC_ECDH_KEY,
    BLE_ECS_EVT_EID_ID_KEY,
    BLE_ECS_EVT_RW_ADV_SLOT,
    BLE_ECS_EVT_RW_ADV_SLOT_PREP, /*used for longs writes*/
    BLE_ECS_EVT_RW_ADV_SLOT_EXEC, /*used for longs writes*/
    BLE_ECS_EVT_FACTORY_RESET,
    BLE_ECS_EVT_REMAIN_CNNTBL
} ble_ecs_evt_type_t;

/**@brief eddystone configuration service init params (corresponds to each char.) */
typedef struct
{
    ble_ecs_brdcst_cap_t        brdcst_cap;
    ble_ecs_active_slot_t       active_slot;
    ble_ecs_adv_intrvl_t        adv_intrvl;
    ble_ecs_radio_tx_pwr_t      radio_tx_pwr;
    ble_ecs_adv_tx_pwr_t        adv_tx_pwr;
    ble_ecs_lock_state_t        lock_state;
    ble_ecs_unlock_t            unlock;
    ble_ecs_public_ecdh_key_t   pub_ecdh_key;
    ble_ecs_eid_id_key_t        eid_id_key;
    ble_ecs_rw_adv_slot_t       rw_adv_slot;
    ble_ecs_factory_reset_t     factory_reset;
    ble_ecs_remain_conntbl_t    remain_cnntbl;
} ble_ecs_init_params_t;

/*Forward Declaration of of ble_ecs_t type*/
typedef struct ble_ecs_s ble_ecs_t;

typedef void (*ble_ecs_write_evt_handler_t) (ble_ecs_t *        p_ecs,
                                             ble_ecs_evt_type_t evt_type,
                                             uint16_t           value_handle,
                                             uint8_t *          p_data,
                                             uint16_t           length);

typedef void (*ble_ecs_read_evt_handler_t) ( ble_ecs_t *                p_ecs,
                                             ble_ecs_evt_type_t         evt_type,
                                             uint16_t                   value_handle
                                             );

/**@brief Eddystone Configuration Service initialization structure.
*
* @details This structure contains the initialization information for the service. The application
* must fill this structure and pass it to the service using the @ref ble_ecs_init
* function.
*/
typedef struct
{
    ble_ecs_init_params_t         * p_init_vals;
    ble_ecs_write_evt_handler_t     write_evt_handler;  /**< Event handler to be called for authorizing write requests. */
    ble_ecs_read_evt_handler_t      read_evt_handler;   /**< Event handler to be called for authorizing read requests. */
} ble_ecs_init_t;

struct ble_ecs_s
{
    uint8_t                         uuid_type;                    /**< UUID type for Eddystone Configuration Service Base UUID. */
    uint16_t                        service_handle;               /**< Handle of Eddystone Configuration Service (as provided by the S110 SoftDevice). */
    ble_gatts_char_handles_t        brdcst_cap_handles;           /**< Handles related to the brdcst_cap characteristic (as provided by the S132 SoftDevice). */
    ble_gatts_char_handles_t        active_slot_handles;          /**< Handles related to the active_slot characteristic (as provided by the S132 SoftDevice). */
    ble_gatts_char_handles_t        adv_intrvl_handles;           /**< Handles related to the adv_intrvl characteristic (as provided by the S132 SoftDevice). */
    ble_gatts_char_handles_t        radio_tx_pwr_handles;         /**< Handles related to the radio_tx_pwr characteristic (as provided by the S132 SoftDevice). */
    ble_gatts_char_handles_t        adv_tx_pwr_handles;           //...
    ble_gatts_char_handles_t        lock_state_handles;           //...
    ble_gatts_char_handles_t        unlock_handles;               //...
    ble_gatts_char_handles_t        pub_ecdh_key_handles;         //...
    ble_gatts_char_handles_t        eid_id_key_handles;           //...
    ble_gatts_char_handles_t        rw_adv_slot_handles;          //...
    ble_gatts_char_handles_t        factory_reset_handles;        //...
    ble_gatts_char_handles_t        remain_cnntbl_handles;        //...
    uint16_t                        conn_handle;                  /**< Handle of the current connection (as provided by the S132 SoftDevice). BLE_CONN_HANDLE_INVALID if not in a connection. */
    ble_ecs_write_evt_handler_t     write_evt_handler;            /**< Event handler to be called for handling write attempts. */
    ble_ecs_read_evt_handler_t      read_evt_handler;             /**< Event handler to be called for handling read attempts. */
};

/**@brief Function for initializing the Eddystone Configuration Service.
 *
 * @param[out] p_ecs      Eddystone Configuration Service structure. This structure must be supplied
 *                        by the application. It is initialized by this function and will
 *                        later be used to identify this particular service instance.
 * @param[in] p_ecs_init  Information needed to initialize the service.
 *
 * @retval NRF_SUCCESS If the service was successfully initialized. Otherwise, an error code is returned.
 * @retval NRF_ERROR_NULL If either of the pointers p_ecs or p_ecs_init is NULL.
 */
uint32_t ble_ecs_init(ble_ecs_t * p_ecs, const ble_ecs_init_t * p_ecs_init);

/**@brief Function for handling the Eddystone Configuration Service's BLE events.
 *
 * @details The Eddystone Configuration Service expects the application to call this function each time an
 * event is received from the S132 SoftDevice. This function processes the event if it
 * is relevant and calls the Eddystone Configuration Service event handler of the
 * application if necessary.
 *
 * @param[in] p_ecs       Eddystone Configuration Service structure.
 * @param[in] p_ble_evt   Event received from the S110 SoftDevice.
 */
void ble_ecs_on_ble_evt(ble_ecs_t * p_ecs, ble_evt_t * p_ble_evt);



#endif //BLE_ECS_H__
