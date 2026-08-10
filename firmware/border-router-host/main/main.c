/*
 * app_main (Plan 05): only init the platform + frame TCP transport.
 * Full boot (W5500 backhaul, RCP SPI, OpenThread, LED, boot button, SRP) -> Plan 07.
 */

#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "frame_tcp.h"

static const char *TAG = "main";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_netif_init());

    err = frame_tcp_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "frame_tcp_init failed %d", err);
        return;
    }
    ESP_LOGI(TAG, "BR host up (transport only), frame TCP port %d", CONFIG_BR_FRAME_TCP_PORT);
}
