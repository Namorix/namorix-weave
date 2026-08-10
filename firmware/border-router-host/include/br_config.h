/*
 * BR firmware centralized config: OpenThread platform macros (RCP SPI radio) and
 * task names/stacks/priorities. Rule 11: UPPER_SNAKE_CASE macros.
 * Note: task names must be <= 15 chars (configMAX_TASK_NAME_LEN = 16).
 */

#pragma once

#include <stddef.h>

#include "sdkconfig.h"

#include "driver/spi_common.h"
#include "driver/spi_master.h"

/* ---- RCP radio config (SPI) — passed to esp_openthread_start() ---- */
#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG() \
    { \
        .radio_mode = RADIO_MODE_SPI_RCP, \
        .radio_spi_config = { \
            .host_device = (spi_host_device_t)CONFIG_BR_RCP_SPI_HOST, \
            .dma_channel = SPI_DMA_CH_AUTO, \
            .spi_interface = { \
                .miso_io_num = CONFIG_BR_RCP_SPI_MISO_GPIO, \
                .mosi_io_num = CONFIG_BR_RCP_SPI_MOSI_GPIO, \
                .sclk_io_num = CONFIG_BR_RCP_SPI_SCLK_GPIO, \
                .quadwp_io_num = -1, \
                .quadhd_io_num = -1, \
            }, \
            .spi_device = { \
                .clock_speed_hz = CONFIG_BR_RCP_SPI_CLOCK_MHZ * 1000 * 1000, \
                .spics_io_num = CONFIG_BR_RCP_SPI_CS_GPIO, \
                .queue_size = 4, \
                .mode = 0, \
            }, \
            .intr_pin = (CONFIG_BR_RCP_SPI_IRQ_GPIO >= 0) ? (gpio_num_t)CONFIG_BR_RCP_SPI_IRQ_GPIO : (gpio_num_t)-1, \
        }, \
    }

#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG() \
    { \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE, \
    }

#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG() \
    { \
        .storage_partition_name = "nvs", \
        .netif_queue_size = 10, \
        .task_queue_size = 10, \
    }

/* ---- Task names (used in xTaskCreate and xTaskGetHandle) ---- */
#define TASK_NAME_MAIN       "main"
#define TASK_NAME_TCP_ACCEPT "tcp_accept"
#define TASK_NAME_TCP_RX     "tcp_rx"
#define TASK_NAME_STATE_WD   "state_wd"
#define TASK_NAME_BOOT_BTN   "boot_btn"
#define TASK_NAME_LED_STATUS "led_status"
#define TASK_NAME_STK_MON    "stk_mon"
#define TASK_NAME_OT_CHANGE  "ot_change"

/* ---- Task stack sizes (bytes) ---- */
#define TASK_STACK_MAIN        CONFIG_ESP_MAIN_TASK_STACK_SIZE
#define TASK_STACK_TCP_ACCEPT  4096
#define TASK_STACK_TCP_RX      4096
#define TASK_STACK_STATE_WD    3072
#define TASK_STACK_BOOT_BTN    4096
#define TASK_STACK_LED_STATUS  2048
#define TASK_STACK_STK_MON     3072
#define TASK_STACK_OT_CHANGE   10240

/* ---- Task priorities ---- */
#define TASK_PRIO_TCP_ACCEPT   4
#define TASK_PRIO_TCP_RX       5
#define TASK_PRIO_STATE_WD     3
#define TASK_PRIO_BOOT_BTN     4
#define TASK_PRIO_LED_STATUS   5
#define TASK_PRIO_STK_MON      2
#define TASK_PRIO_OT_CHANGE    4

/* ---- Shared task table ---- */
/*
 * All tasks we create (BR_HEALTH handler + stack monitor). Defined in main.c
 * (extern here so TUs that include br_config.h but don't use the table, e.g.
 * ot_change_detector.c, do not get an unused-variable warning).
 */
typedef struct {
    const char *name;
    uint32_t    stack;
} br_task_info_t;

extern const br_task_info_t k_br_tasks[];
extern const size_t k_br_tasks_count;
