/*
 * Boot button: detect a long press on a GPIO and fire the on_long_press callback.
 */

#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "br_config.h"
#include "hardware/boot_btn.h"

static const char *TAG = "boot_btn";

#define DEFAULT_STACK_SIZE TASK_STACK_BOOT_BTN
#define DEFAULT_PRIORITY   TASK_PRIO_BOOT_BTN

static void boot_btn_task(void *pv)
{
    boot_btn_config_t config;
    memcpy(&config, pv, sizeof(config));
    vPortFree(pv);

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << config.gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    uint32_t hold_ticks = 0;
    const uint32_t hold_count = (config.hold_ms + config.poll_ms - 1) / config.poll_ms;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(config.poll_ms));
        if (gpio_get_level(config.gpio_num) == BOOT_BTN_ACTIVE_LEVEL) {
            hold_ticks++;
            if (hold_ticks >= hold_count) {
                ESP_LOGW(TAG, "long press GPIO %d (~%lu s)", config.gpio_num, (unsigned long)(config.hold_ms / 1000));
                if (config.on_long_press) {
                    config.on_long_press(config.ctx);
                }
                hold_ticks = 0;
            }
        } else {
            hold_ticks = 0;
        }
    }
}

esp_err_t boot_btn_start(const boot_btn_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    boot_btn_config_t *copy = (boot_btn_config_t *)pvPortMalloc(sizeof(*copy));
    if (copy == NULL) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(copy, config, sizeof(*copy));

    uint32_t stack = copy->task_stack_size > 0 ? copy->task_stack_size : DEFAULT_STACK_SIZE;
    UBaseType_t prio = copy->task_priority > 0 ? copy->task_priority : DEFAULT_PRIORITY;

    BaseType_t ok = xTaskCreate(boot_btn_task, TASK_NAME_BOOT_BTN, stack, copy, prio, NULL);
    if (ok != pdPASS) {
        vPortFree(copy);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
