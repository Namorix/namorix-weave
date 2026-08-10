/*
 * Ethernet W5500 (SPI) backhaul.
 * Init driver + netif, wait for IPv4 (DHCP), return the netif for the backbone.
 * LAN only, no Wi-Fi fallback.
 */

#include "backhaul/eth_w5500.h"
#include "esp_log.h"
#include "esp_eth.h"
#include "esp_eth_mac_spi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_netif_ip_addr.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "sdkconfig.h"
#include "esp_mac.h"
#include <string.h>

#if CONFIG_BR_ETH_W5500_ENABLE

#define TAG "eth_w5500"

#define ETH_GOT_IP_BIT   BIT0
#define ETH_GOT_IP6_BIT  BIT1

static EventGroupHandle_t s_eth_event_group;
static esp_netif_t *s_eth_netif = NULL;
static esp_eth_handle_t s_eth_handle = NULL;
static esp_eth_netif_glue_handle_t s_glue = NULL;

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    esp_netif_t *netif = (esp_netif_t *)arg;

    if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "Ethernet link up");
        if (netif != NULL) {
            esp_err_t err = esp_netif_create_ip6_linklocal(netif);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "failed to create IPv6 link-local address: %s", esp_err_to_name(err));
            }
        }
    } else if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_DISCONNECTED) {
        ESP_LOGW(TAG, "Ethernet link down");
    }
}

static void got_ip_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Ethernet got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_eth_event_group, ETH_GOT_IP_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_GOT_IP6) {
        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        if (event->esp_netif == s_eth_netif) {
            ESP_LOGI(TAG, "Ethernet got IPv6: " IPV6STR, IPV62STR(event->ip6_info.ip));
            xEventGroupSetBits(s_eth_event_group, ETH_GOT_IP6_BIT);
        }
    }
}

esp_err_t eth_w5500_init(void)
{
    s_eth_event_group = xEventGroupCreate();
    if (s_eth_event_group == NULL) {
        ESP_LOGE(TAG, "EventGroup create failed");
        return ESP_ERR_NO_MEM;
    }

    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_BR_ETH_SPI_MISO_GPIO,
        .mosi_io_num = CONFIG_BR_ETH_SPI_MOSI_GPIO,
        .sclk_io_num = CONFIG_BR_ETH_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    esp_err_t err = spi_bus_initialize(CONFIG_BR_ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init %s", esp_err_to_name(err));
        vEventGroupDelete(s_eth_event_group);
        return err;
    }

#if CONFIG_BR_ETH_RST_GPIO >= 0
    /* Reset W5500: hold low, delay, release, delay (timing from Kconfig). */
    gpio_reset_pin((gpio_num_t)CONFIG_BR_ETH_RST_GPIO);
    gpio_set_direction((gpio_num_t)CONFIG_BR_ETH_RST_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)CONFIG_BR_ETH_RST_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(CONFIG_BR_ETH_RST_HOLD_MS));
    gpio_set_level((gpio_num_t)CONFIG_BR_ETH_RST_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(CONFIG_BR_ETH_RST_RELEASE_MS));
    ESP_LOGI(TAG, "W5500 RST: hold %d ms, release delay %d ms",
             (int)CONFIG_BR_ETH_RST_HOLD_MS, (int)CONFIG_BR_ETH_RST_RELEASE_MS);
#endif

    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = CONFIG_BR_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size = 20,
        .spics_io_num = CONFIG_BR_ETH_SPI_CS_GPIO,
    };

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    /* Reset already done by hand above with the Kconfig timing; driver must not reset again. */
    phy_config.reset_gpio_num = -1;

    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(CONFIG_BR_ETH_SPI_HOST, &devcfg);
    w5500_config.int_gpio_num = CONFIG_BR_ETH_INT_GPIO;
    w5500_config.poll_period_ms = (CONFIG_BR_ETH_INT_GPIO >= 0) ? 0 : CONFIG_BR_ETH_POLLING_MS;

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    if (mac == NULL) {
        ESP_LOGE(TAG, "esp_eth_mac_new_w5500 failed");
        spi_bus_free(CONFIG_BR_ETH_SPI_HOST);
        vEventGroupDelete(s_eth_event_group);
        return ESP_FAIL;
    }

    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    if (phy == NULL) {
        mac->del(mac);
        spi_bus_free(CONFIG_BR_ETH_SPI_HOST);
        vEventGroupDelete(s_eth_event_group);
        return ESP_FAIL;
    }

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    err = esp_eth_driver_install(&eth_config, &s_eth_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_eth_driver_install %s", esp_err_to_name(err));
        phy->del(phy);
        mac->del(mac);
        spi_bus_free(CONFIG_BR_ETH_SPI_HOST);
        vEventGroupDelete(s_eth_event_group);
        return err;
    }

    uint8_t base_mac[6];
    err = esp_efuse_mac_get_default(base_mac);
    if (err == ESP_OK) {
        uint8_t local_mac[6];
        esp_derive_local_mac(local_mac, base_mac);
        esp_eth_ioctl(s_eth_handle, ETH_CMD_S_MAC_ADDR, local_mac);
    }

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_eth_netif = esp_netif_new(&netif_cfg);
    if (s_eth_netif == NULL) {
        esp_eth_driver_uninstall(s_eth_handle);
        phy->del(phy);
        mac->del(mac);
        spi_bus_free(CONFIG_BR_ETH_SPI_HOST);
        vEventGroupDelete(s_eth_event_group);
        s_eth_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_glue = esp_eth_new_netif_glue(s_eth_handle);
    if (s_glue == NULL) {
        esp_netif_destroy(s_eth_netif);
        esp_eth_driver_uninstall(s_eth_handle);
        phy->del(phy);
        mac->del(mac);
        spi_bus_free(CONFIG_BR_ETH_SPI_HOST);
        vEventGroupDelete(s_eth_event_group);
        s_eth_netif = NULL;
        s_eth_handle = NULL;
        return ESP_FAIL;
    }

    err = esp_netif_attach(s_eth_netif, s_glue);
    if (err != ESP_OK) {
        esp_eth_del_netif_glue(s_glue);
        esp_netif_destroy(s_eth_netif);
        esp_eth_driver_uninstall(s_eth_handle);
        phy->del(phy);
        mac->del(mac);
        spi_bus_free(CONFIG_BR_ETH_SPI_HOST);
        vEventGroupDelete(s_eth_event_group);
        s_eth_netif = NULL;
        s_eth_handle = NULL;
        s_glue = NULL;
        return err;
    }

    err = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, s_eth_netif);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register ETH_EVENT %s", esp_err_to_name(err));
        goto fail;
    }

    err = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_handler, NULL);
    if (err != ESP_OK) {
        esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler);
        goto fail;
    }

    err = esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &got_ip_handler, NULL);
    if (err != ESP_OK) {
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, got_ip_handler);
        esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler);
        goto fail;
    }

    err = esp_eth_start(s_eth_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_eth_start %s", esp_err_to_name(err));
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_GOT_IP6, got_ip_handler);
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, got_ip_handler);
        esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler);
        goto fail;
    }

    /* Wait for IPv4 (DHCP); IPv6 alone is not enough — the backend needs IPv4 to reach the BR. */
    ESP_LOGI(TAG, "waiting for Ethernet IPv4 (DHCP, timeout %d ms)...", (int)CONFIG_BR_ETH_LINK_TIMEOUT_MS);
    EventBits_t bits = xEventGroupWaitBits(s_eth_event_group,
                                            ETH_GOT_IP_BIT,
                                            pdFALSE, pdFALSE,
                                            pdMS_TO_TICKS(CONFIG_BR_ETH_LINK_TIMEOUT_MS));
    if (bits & ETH_GOT_IP_BIT) {
        ESP_LOGI(TAG, "Ethernet W5500 init OK (IPv4)");
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Ethernet IPv4 timeout");
    esp_eth_stop(s_eth_handle);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_GOT_IP6, got_ip_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, got_ip_handler);
    esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler);
    esp_eth_del_netif_glue(s_glue);
    esp_netif_destroy(s_eth_netif);
    esp_eth_driver_uninstall(s_eth_handle);
    phy->del(phy);
    mac->del(mac);
    spi_bus_free(CONFIG_BR_ETH_SPI_HOST);
    vEventGroupDelete(s_eth_event_group);
    s_eth_netif = NULL;
    s_eth_handle = NULL;
    s_glue = NULL;
    return ESP_ERR_TIMEOUT;

fail:
    esp_eth_del_netif_glue(s_glue);
    esp_netif_destroy(s_eth_netif);
    esp_eth_driver_uninstall(s_eth_handle);
    phy->del(phy);
    mac->del(mac);
    spi_bus_free(CONFIG_BR_ETH_SPI_HOST);
    vEventGroupDelete(s_eth_event_group);
    s_eth_netif = NULL;
    s_eth_handle = NULL;
    s_glue = NULL;
    return err;
}

esp_netif_t *eth_w5500_get_netif(void)
{
    return s_eth_netif;
}

#else /* !CONFIG_BR_ETH_W5500_ENABLE */

esp_err_t eth_w5500_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_netif_t *eth_w5500_get_netif(void)
{
    return NULL;
}

#endif /* CONFIG_BR_ETH_W5500_ENABLE */
