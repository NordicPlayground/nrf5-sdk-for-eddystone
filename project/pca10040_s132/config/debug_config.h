#ifndef DEBUG_CONFIG_H__
#define DEBUG_CONFIG_H__

/*Uncomment if needing printfs to SEGGER_RTT from different modules*/
// #define SLOT_DEBUG
// #define ADV_DEBUG
// #define BLE_HANDLER_DEBUG
// #define FLASH_DEBUG
// #define SECURITY_DEBUG
// #define TLM_DEBUG

/* Uncomment to Erase All Flash when board is reset */
// #define ERASE_FLASH_ON_REBOOT

/*@note keep this macro defined in order to comply with
Eddystone specification of randomizing the device address
upon EID generation. Only comment out for debugging purposes!*/
#define RANDOMIZE_MAC

#endif /*DEBUG_CONFIG_H__*/
