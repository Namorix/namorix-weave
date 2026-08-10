/*
 * app_main: full BR boot flow — platform, W5500 backhaul (IPv4 DHCP), mDNS,
 * RCP reset, OpenThread start (leader weight boost), change detector, dataset
 * init, border router + SRP server, frame TCP transport, LED, boot button,
 * stack monitor. Order mirrors the working reference br-host firmware.
 */

#include <stdint.h>
#include <string.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_log_level.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_vfs_eventfd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "esp_openthread.h"
#include "esp_openthread_border_router.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"
#include "openthread/srp_server.h"

#include "br_config.h"
#include "backhaul/eth_w5500.h"
#include "command/command.h"
#include "../include/transport/frame_tcp.h"
#include "rcp/rcp_ctrl.h"
#include "openthread/ot_launch.h"
#include "openthread/dataset_init.h"
#include "openthread/ot_change_detector.h"
#include "hardware/led_status.h"
#include "hardware/boot_btn.h"

#define TAG "main"

#define BOOT_BTN_GPIO     0     /* BOOT button on ESP32-S3 DevKit */
#define BOOT_BTN_HOLD_MS  3000  /* ~3s hold = long press */

#define STACK_MONITOR_INTERVAL_MS 30000

/* Shared task table (br_task_info_t in br_config.h): consumed by the BR_HEALTH
 * handler and this stack monitor. */
const br_task_info_t k_br_tasks[] = {
    { TASK_NAME_MAIN,       TASK_STACK_MAIN       },
    { TASK_NAME_TCP_ACCEPT, TASK_STACK_TCP_ACCEPT },
    { TASK_NAME_TCP_RX,     TASK_STACK_TCP_RX     },
    { TASK_NAME_STATE_WD,   TASK_STACK_STATE_WD   },
    { TASK_NAME_BOOT_BTN,   TASK_STACK_BOOT_BTN   },
    { TASK_NAME_LED_STATUS, TASK_STACK_LED_STATUS },
    { TASK_NAME_STK_MON,    TASK_STACK_STK_MON    },
    { TASK_NAME_OT_CHANGE,  TASK_STACK_OT_CHANGE  },
};
const size_t k_br_tasks_count = sizeof(k_br_tasks) / sizeof(k_br_tasks[0]);

static void __attribute__((unused)) stack_monitor_task(void *pv)
{
    (void)pv;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(STACK_MONITOR_INTERVAL_MS));
        for (size_t i = 0; i < k_br_tasks_count; i++) {
            TaskHandle_t h = xTaskGetHandle(k_br_tasks[i].name);
            if (h == NULL) {
                continue;
            }
            UBaseType_t hwm  = uxTaskGetStackHighWaterMark(h);
            uint32_t    used = k_br_tasks[i].stack > (uint32_t)hwm
                                   ? k_br_tasks[i].stack - (uint32_t)hwm
                                   : 0;
            ESP_LOGI(TAG, "stack hwm | %-22s high_water_mark=%4u bytes (used ~%4u / %u)",
                     k_br_tasks[i].name, (unsigned)hwm, (unsigned)used, (unsigned)k_br_tasks[i].stack);
        }
        ESP_LOGI(TAG, "heap      | free=%u bytes  min_free=%u bytes",
                 (unsigned)esp_get_free_heap_size(),
                 (unsigned)esp_get_minimum_free_heap_size());
    }
}

static void on_boot_long_press(void *ctx)
{
    (void)ctx;
    ESP_LOGW(TAG, "boot button long press: factory reset");
    command_factory_reset();
}

void app_main(void)
{
    esp_log_level_set("OPENTHREAD", ESP_LOG_WARN);

    /* eventfd: netif, task queue, border router; +1 when RCP SPI (Spinel) is used. */
    size_t max_eventfd = 3;
#if CONFIG_OPENTHREAD_RADIO_SPINEL_SPI
    max_eventfd++;
#endif
    esp_vfs_eventfd_config_t eventfd_config = { .max_fds = max_eventfd };

    esp_openthread_config_t openthread_config = {
        .netif_config = ESP_NETIF_DEFAULT_OPENTHREAD(),
        .platform_config = {
            .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
            .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
            .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
        },
    };
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Backhaul: LAN only (Ethernet W5500). IPv4 DHCP timeout -> restart. */
    esp_netif_t *backbone = NULL;
#if CONFIG_BR_ETH_W5500_ENABLE
    err = eth_w5500_init();
    if (err == ESP_OK) {
        backbone = eth_w5500_get_netif();
    } else if (err == ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "Ethernet IPv4 timeout, restarting...");
        esp_restart();
    }
#endif
    if (backbone != NULL) {
        esp_openthread_set_backbone_netif(backbone);
    }

    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set("Border-Router-Host"));
    /* Advertise the frame port over mDNS so the backend can discover BR_IP:port. */
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_thread-frame", "_tcp", CONFIG_BR_FRAME_TCP_PORT, NULL, 0));

    /* RCP control pins (RESET/BOOT): reset the RCP for a clean state. */
    ESP_ERROR_CHECK(rcp_ctrl_init());
    rcp_ctrl_reset();
    vTaskDelay(pdMS_TO_TICKS(500));  /* wait for the RCP (H2) to boot and become ready */

    ESP_LOGI(TAG, "RCP over SPI: host=%d SCLK=%d MOSI=%d MISO=%d CS=%d IRQ=%d clk=%d MHz",
             (int)CONFIG_BR_RCP_SPI_HOST, CONFIG_BR_RCP_SPI_SCLK_GPIO, CONFIG_BR_RCP_SPI_MOSI_GPIO,
             CONFIG_BR_RCP_SPI_MISO_GPIO, CONFIG_BR_RCP_SPI_CS_GPIO, CONFIG_BR_RCP_SPI_IRQ_GPIO,
             CONFIG_BR_RCP_SPI_CLOCK_MHZ);

    ot_launch_start(&openthread_config);

    /* Change detector: debounce + snapshot diff + CMD_NOTIFY push. */
    otInstance *instance = esp_openthread_get_instance();
    if (instance != NULL) {
        if (!ot_change_detector_init(instance)) {
            ESP_LOGW(TAG, "ot_change_detector_init failed");
        }
    } else {
        ESP_LOGW(TAG, "ot instance not ready, skip change detector init");
    }

    /* If no active dataset yet, create one (ESP-BR-<MAC>) and set it active. */
    dataset_init_on_boot();

    /* Border routing + prefix delegation (after OT start). */
    if (backbone != NULL) {
        esp_openthread_lock_acquire(portMAX_DELAY);
        esp_err_t br_err = esp_openthread_border_router_init();
        esp_openthread_lock_release();
        if (br_err != ESP_OK) {
            ESP_LOGE(TAG, "esp_openthread_border_router_init %s", esp_err_to_name(br_err));
        } else {
            ESP_LOGI(TAG, "border router init OK (routing + prefix)");
        }

        /* SRP server: Thread nodes (SRP client) register services, DNS-based discovery. */
        otInstance *instance = esp_openthread_get_instance();
        if (instance != NULL && esp_openthread_lock_acquire(pdMS_TO_TICKS(1000))) {
            otSrpServerSetEnabled(instance, true);
            esp_openthread_lock_release();
            ESP_LOGI(TAG, "SRP server enabled");
        } else {
            ESP_LOGW(TAG, "SRP server not started (instance or lock failed)");
        }
    }

    /* Frame TCP transport: frame protocol over TCP + state watchdog. */
    err = frame_tcp_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "frame_tcp_init failed %d", err);
        return;
    }

    /* LED status indicator (WS2812). */
    ESP_ERROR_CHECK(led_status_start(NULL));

    /* Boot button: long press ~3s -> factory reset (erase NVS) and restart. */
    boot_btn_config_t btn_cfg = {
        .gpio_num = BOOT_BTN_GPIO,
        .hold_ms = BOOT_BTN_HOLD_MS,
        .poll_ms = 50,
        .on_long_press = on_boot_long_press,
        .ctx = NULL,
        .task_stack_size = TASK_STACK_BOOT_BTN,
        .task_priority = TASK_PRIO_BOOT_BTN,
    };
    ESP_ERROR_CHECK(boot_btn_start(&btn_cfg));

    xTaskCreate(stack_monitor_task, TASK_NAME_STK_MON, TASK_STACK_STK_MON, NULL, TASK_PRIO_STK_MON, NULL);
}
