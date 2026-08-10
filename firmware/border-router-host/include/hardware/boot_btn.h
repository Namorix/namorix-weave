/*
 * Boot button: detect a long press on a GPIO.
 * Used for factory reset or other functionality (on_long_press callback).
 */

#ifndef HARDWARE_BOOT_BTN_H
#define HARDWARE_BOOT_BTN_H

#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Logic level when the button is pressed (0 = active low with pull-up). */
#ifndef BOOT_BTN_ACTIVE_LEVEL
#define BOOT_BTN_ACTIVE_LEVEL 0
#endif

/* Long press callback: (ctx). */
typedef void (*boot_btn_long_press_cb_t)(void *ctx);

/* Boot button config. */
typedef struct {
    int gpio_num;                    /* GPIO of the button (input, pull-up). */
    uint32_t hold_ms;                /* Hold time (ms) to count as a long press. */
    uint32_t poll_ms;                /* Poll period (ms). */
    boot_btn_long_press_cb_t on_long_press; /* Callback on long press; can be NULL. */
    void *ctx;                       /* Context passed to on_long_press. */
    uint32_t task_stack_size;        /* Task stack size; 0 = default 4096. */
    UBaseType_t task_priority;       /* Task priority; 0 = default 4. */
} boot_btn_config_t;

/*
 * Initialize and run the button-reading task; on long press call on_long_press(ctx).
 *
 * @param config Config (gpio_num, hold_ms, poll_ms, on_long_press, ctx, ...).
 * @return ESP_OK on success.
 */
esp_err_t boot_btn_start(const boot_btn_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* HARDWARE_BOOT_BTN_H */
