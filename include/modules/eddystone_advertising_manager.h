#ifndef EDDYSTONE_ADVERTISING_MANAGER_H
#define EDDYSTONE_ADVERTISING_MANAGER_H

#include "eddystone_adv_slot.h"

typedef enum
{
    EDDYSTONE_BLE_ADV_CONNECTABLE_FALSE,
    EDDYSTONE_BLE_ADV_CONNECTABLE_TRUE
}eddystone_ble_adv_connectable_t;

/** @brief Function for initializing the advertising manager
* @param[in] ecs_uuid_type     ECS UUID type used for advertising ECS UUID
*/
void eddystone_advertising_manager_init( uint8_t ecs_uuid_type );

/** @brief Function for passing ble evts to the advertising manager
 * @param[in] p_ble_evt     pointer to the ble evt
 */
void eddystone_advertising_manager_on_ble_evt( ble_evt_t * p_ble_evt );

#endif /*EDDYSTONE_ADVERTISING_MANAGER_H*/
