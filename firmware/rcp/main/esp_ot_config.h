/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * OpenThread Radio Co-Processor (RCP) — giao tiếp Host chỉ qua SPI.
 * UART dành sau cho flash/update RCP.
 */

#pragma once

#include "sdkconfig.h"

#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG()                   \
    {                                                           \
        .radio_mode = RADIO_MODE_NATIVE,                        \
    }


#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG()                    \
    {                                                           \
        .host_connection_mode = HOST_CONNECTION_MODE_RCP_SPI,   \
        .spi_slave_config = {                                   \
            .host_device = SPI2_HOST,                           \
            .bus_config = {                                     \
                .mosi_io_num = CONFIG_RCP_SPI_MOSI_GPIO,        \
                .miso_io_num = CONFIG_RCP_SPI_MISO_GPIO,        \
                .sclk_io_num = CONFIG_RCP_SPI_SCLK_GPIO,        \
                .quadwp_io_num = -1,                            \
                .quadhd_io_num = -1,                            \
                .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,       \
            },                                                  \
            .slave_config = {                                   \
                .mode = 0,                                      \
                .spics_io_num = CONFIG_RCP_SPI_CS_GPIO,         \
                .queue_size = 3,                                \
                .flags = 0,                                     \
            },                                                  \
            .intr_pin = (CONFIG_RCP_SPI_IRQ_GPIO >= 0) ? (gpio_num_t)CONFIG_RCP_SPI_IRQ_GPIO : (gpio_num_t)-1, \
        },                                                      \
    }

#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG()    \
    {                                           \
        .storage_partition_name = "nvs",        \
        .netif_queue_size = 10,                 \
        .task_queue_size = 10,                  \
    }
