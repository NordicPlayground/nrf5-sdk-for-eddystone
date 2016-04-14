#ifndef EDDYSTONE_APP_CONFIG_H
#define EDDYSTONE_APP_CONFIG_H

#include "eddystone.h"
#include "boards.h"
#include "app_timer_appsh.h"

//HW CONFIGS
#define REGISTRATION_BUTTON                             BUTTON_1                         /**< Button to push when putting the beacon in registration mode */
#define USE_ECB_ENCRYPT_HW                              1                                /**< Configure between using the hardware peripheral or software library for ECB encyrption (decryption is always SW) */

//TIMER CONFIGS
#define APP_TIMER_PRESCALER                             0                                 /**< Value of the RTC1 PRESCALER register. 4095 = 125 ms every tick */
#define APP_TIMER_OP_QUEUE_SIZE                         10                                /**< Size of timer operation queues. */

//SCHEDULER CONFIGS
#define SCHED_MAX_EVENT_DATA_SIZE                       sizeof(app_timer_event_t)
#define SCHED_QUEUE_SIZE                                10

//BLE CONFIGS
#define IS_SRVC_CHANGED_CHARACT_PRESENT                 0                                 /**< Include the service changed characteristic. If not enabled, the server's database cannot be changed for the lifetime of the device. */

#define CENTRAL_LINK_COUNT                              0                                 /**<number of central links used by the application. When changing this number remember to adjust the RAM settings*/
#define PERIPHERAL_LINK_COUNT                           1                                 /**<number of peripheral links used by the application. When changing this number remember to adjust the RAM settings*/

#define APP_CFG_NON_CONN_ADV_TIMEOUT                    0                               /**< Time for which the device must be advertising in non-connectable mode (in seconds). 0 disables the time-out. */
#define DEFAULT_NON_CONNECTABLE_ADV_INTERVAL_MS         1000                            /**< The advertising interval for non-connectable advertisement (1000 ms). This value can vary between 100 ms and 10.24 s). */
#define APP_CFG_CONNECTABLE_ADV_TIMEOUT                 60                              /**< Time for which the device must be advertising in connectable mode (in seconds). 0 disables the time-out. */
#define DEFAULT_CONNECTABLE_ADV_INTERVAL_MS             100                             /**< The advertising interval for connectable advertisement (1000 ms). This value can vary between 20 ms and 10.24 s). */

#define DEFAULT_NON_CONNECTABLE_TX_POWER                0
#define DEFAULT_CONNECTABLE_TX_POWER                    0

//EDDYSTONE CONFIGS
#define APP_MAX_ADV_SLOTS                               5
#define APP_MAX_EID_SLOTS                               APP_MAX_ADV_SLOTS  //DO NOT CHANGE THIS

#define APP_IS_VARIABLE_ADV_SUPPORTED                   ECS_BRDCST_VAR_ADV_SUPPORTED_No
#define APP_IS_VARIABLE_TX_POWER_SUPPORTED              ECS_BRDCST_VAR_TX_POWER_SUPPORTED_Yes

// Eddystone common data
#define APP_EDDYSTONE_UUID              0xFEAA                            /**< UUID for Eddystone beacons according to specification. */
#define APP_EDDYSTONE_RSSI              0xEE                              /**< 0xEE = -18 dB is the approximate signal strength at 0 m. */

// Eddystone UID data
#define APP_EDDYSTONE_UID_FRAME_TYPE    0x00                              /**< UID frame type is fixed at 0x00. */
#define APP_EDDYSTONE_UID_NAMESPACE     0xAA, 0xAA, 0xBB, 0xBB, \
                                        0xCC, 0xCC, 0xDD, 0xDD, \
                                        0xEE, 0xEE                        /**< Mock values for 10-byte Eddystone UID ID namespace. */
#define APP_EDDYSTONE_UID_ID            0x01, 0x02, 0x03, 0x04, \
                                        0x05, 0x06                        /**< Mock values for 6-byte Eddystone UID ID instance.  */
#define APP_EDDYSTONE_UID_RFU           0x00, 0x00                        /**< Reserved for future use according to specification. */

// Eddystone URL data
#define APP_EDDYSTONE_URL_FRAME_TYPE    0x10                              /**< URL Frame type is fixed at 0x10. */
#define APP_EDDYSTONE_URL_SCHEME        0x00                              /**< 0x00 = "http://www" URL prefix scheme according to specification. */
#define APP_EDDYSTONE_URL_URL           0x6e, 0x6f, 0x72, 0x64, \
                                        0x69, 0x63, 0x73, 0x65, \
                                        0x6d,0x69, 0x00                   /**< "nordicsemi.com". Last byte suffix 0x00 = ".com" according to specification. */
// Eddystone TLM data
#define APP_EDDYSTONE_TLM_FRAME_TYPE    0x20                              /**< TLM frame type is fixed at 0x20. */
#define APP_EDDYSTONE_TLM_VERSION       0x00                              /**< TLM version might change in the future to accommodate other data according to specification. */
#define APP_EDDYSTONE_TLM_BATTERY       0x00, 0xF0                        /**< Mock value. Battery voltage in 1 mV per bit. */
#define APP_EDDYSTONE_TLM_TEMPERATURE   0x0F, 0x00                        /**< Mock value. Temperature [C]. Signed 8.8 fixed-point notation. */
#define APP_EDDYSTONE_TLM_ADV_COUNT     0x00, 0x00, 0x00, 0x00            /**< Running count of advertisements of all types since power-up or reboot. */
#define APP_EDDYSTONE_TLM_SEC_COUNT     0x00, 0x00, 0x00, 0x00            /**< Running count in 0.1 s resolution since power-up or reboot. */

#define DEFAULT_FRAME_TYPE                      APP_EDDYSTONE_URL_FRAME_TYPE
#define DEFAULT_FRAME_DATA                      {APP_EDDYSTONE_URL_SCHEME, APP_EDDYSTONE_URL_URL} /**Should mimic the data that would be written to
                                                                                                                                  the RW ADV slot characteristic (e.g. no RSSI/DFU for UID) */




#endif /*EDDYSTONE_APP_CONFIG_H*/
