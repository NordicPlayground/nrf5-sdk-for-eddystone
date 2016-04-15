#include "eddystone_registration_ui.h"
#include "app_button.h"
#include "app_error.h"
#include "app_timer.h"
#include "eddystone_app_config.h"

#define RETURN_IF_ERROR(x) if(x != NRF_SUCCESS) return x

static eddystone_registration_start_cb_t adv_start_callback;

/**@brief Function for handling button events from app_button IRQ
 *
 * @param[in] pin_no        Pin of the button for which an event has occured
 * @param[in] button_action Press or Release
 */
static void eddystone_button_evt_handler(uint8_t pin_no, uint8_t button_action)
{
    if (button_action == APP_BUTTON_PUSH && pin_no == BUTTON_1)
    {
        adv_start_callback();
    }
}

/**
 * @brief Function for initializing the registation button
 *
 * @retval Values returned by @ref app_button_init
 * @retval Values returned by @ref app_button_enable
 */
static ret_code_t eddystone_registration_button_init(void)
{
    ret_code_t err_code;
    const uint8_t buttons_cnt = 1;
    static app_button_cfg_t buttons_cfgs =
    {
        .pin_no = REGISTRATION_BUTTON,
        .active_state = APP_BUTTON_ACTIVE_LOW,
        .pull_cfg = NRF_GPIO_PIN_PULLUP,
        .button_handler = eddystone_button_evt_handler
    };

    err_code = app_button_init(&buttons_cfgs, buttons_cnt, APP_TIMER_TICKS(100, APP_TIMER_PRESCALER));
    RETURN_IF_ERROR(err_code);
    return app_button_enable();
}

ret_code_t eddystone_registration_ui_init( eddystone_registration_start_cb_t adv_start_cb )
{
    adv_start_callback = adv_start_cb;
    return eddystone_registration_button_init();
}
