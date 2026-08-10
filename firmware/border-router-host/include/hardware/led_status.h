/*
 * LED status for the Thread Border Router on ESP32-S3.
 * - Disabled: red blink
 * - Detached: blue blink
 * - Leader: green solid
 * - Router: purple solid
 * - Child: blue solid
 *
 * Uses a WS2812/WS2812B addressable RGB LED via the RMT peripheral.
 * ESP32-S3 DevKit: GPIO 48 for the onboard LED or GPIO 5 for an external LED.
 */

#ifndef HARDWARE_LED_STATUS_H
#define HARDWARE_LED_STATUS_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Config: WS2812 data GPIO (0 = use default from menuconfig). */
typedef struct {
    int gpio_data;  /* GPIO for the WS2812 data line. 0 = use CONFIG_LED_STATUS_GPIO */
} led_status_config_t;

/*
 * Initialize and run the LED status task.
 *
 * @param config GPIO config. If NULL or gpio_data == 0, uses the menuconfig GPIO.
 * @return ESP_OK on success, an error code otherwise.
 */
esp_err_t led_status_start(const led_status_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* HARDWARE_LED_STATUS_H */
