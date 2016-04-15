#ifndef EDDYSTONE_REGISTRATION_UI_H
#define EDDYSTONE_REGISTRATION_UI_H

#include <stdint.h>

/* @brief Callback for when the UI is triggered*/
typedef void (*eddystone_registration_start_cb_t)(void);

/**
 * @brief Function for initializing the registation button
 *
 * @params[in] adv_start_cb function callback to start a connectable advertisement to begin registration
 *
 * @retval Values returned by @ref app_button_init
 * @retval Values returned by @ref app_button_enable
 */
uint32_t eddystone_registration_ui_init( eddystone_registration_start_cb_t adv_start_cb );

#endif /*EDDYSTONE_REGISTRATION_UI_H*/
