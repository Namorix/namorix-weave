/*
 * Ethernet W5500 (SPI) backhaul.
 * LAN-only backhaul (Ethernet W5500), no Wi-Fi.
 */

#ifndef ETH_W5500_H
#define ETH_W5500_H

#include "esp_err.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize W5500 (SPI), create netif, start driver, wait for IPv4 (DHCP) (Kconfig timeout).
 * Call after esp_netif_init() and esp_event_loop_create_default().
 * @return ESP_OK on link + IP; ESP_ERR_TIMEOUT or other error. */
esp_err_t eth_w5500_init(void);

/* Return the Ethernet netif (after eth_w5500_init() succeeds).
 * @return esp_netif_t* or NULL if not initialized / init failed. */
esp_netif_t *eth_w5500_get_netif(void);

#ifdef __cplusplus
}
#endif

#endif /* ETH_W5500_H */
