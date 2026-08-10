/*
 * RCP control: RESET/BOOT pins of the ESP32-H2 RCP driven from the ESP32-S3 host.
 */

#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "br_config.h"
#include "rcp/rcp_ctrl.h"

#define TAG "rcp_ctrl"

#define RCP_RESET_PIN CONFIG_BR_RCP_RESET_GPIO
#define RCP_BOOT_PIN  CONFIG_BR_RCP_BOOT_GPIO

static bool s_initialized = false;

esp_err_t rcp_ctrl_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    gpio_config_t reset_pin_config = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RCP_RESET_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,  /* pull-up: RESET stays HIGH when not driven */
    };
    ESP_RETURN_ON_ERROR(gpio_config(&reset_pin_config), TAG, "failed to configure RESET pin");

    gpio_config_t boot_pin_config = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RCP_BOOT_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,  /* pull-up: BOOT stays HIGH (boot from flash) */
    };
    ESP_RETURN_ON_ERROR(gpio_config(&boot_pin_config), TAG, "failed to configure BOOT pin");

    /* Initial state: RESET HIGH (not reset), BOOT HIGH (boot from flash). */
    gpio_set_level(RCP_RESET_PIN, 1);
    gpio_set_level(RCP_BOOT_PIN, 1);

    s_initialized = true;
    ESP_LOGI(TAG, "RCP control pins initialized: RESET=GPIO%d, BOOT=GPIO%d", RCP_RESET_PIN, RCP_BOOT_PIN);
    return ESP_OK;
}

void rcp_ctrl_reset(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "rcp_ctrl_init() must be called first");
        return;
    }

    ESP_LOGI(TAG, "resetting RCP...");
    gpio_set_level(RCP_RESET_PIN, 0);  /* pull RESET LOW */
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(RCP_RESET_PIN, 1);  /* pull RESET HIGH */
    vTaskDelay(pdMS_TO_TICKS(30));
    ESP_LOGI(TAG, "RCP reset complete");
}

void rcp_ctrl_enter_download_mode(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "rcp_ctrl_init() must be called first");
        return;
    }

    ESP_LOGI(TAG, "entering RCP download mode...");
    gpio_set_level(RCP_BOOT_PIN, 0);   /* BOOT LOW (download mode) */
    gpio_set_level(RCP_RESET_PIN, 0);  /* RESET LOW */
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(RCP_RESET_PIN, 1);  /* RESET HIGH */
    /* Keep BOOT LOW while flashing. */
    ESP_LOGI(TAG, "RCP in download mode (BOOT=LOW)");
}

void rcp_ctrl_exit_download_mode(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "rcp_ctrl_init() must be called first");
        return;
    }

    ESP_LOGI(TAG, "exiting RCP download mode...");
    gpio_set_level(RCP_BOOT_PIN, 1);   /* BOOT HIGH (boot from flash) */
    gpio_set_level(RCP_RESET_PIN, 0);  /* RESET LOW */
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(RCP_RESET_PIN, 1);  /* RESET HIGH */
    ESP_LOGI(TAG, "RCP booting from flash (BOOT=HIGH)");
}
