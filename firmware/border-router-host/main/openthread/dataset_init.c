/*
 * Dataset init: on boot check the active dataset; if none, create a random one
 * (network name ESP-BR-<MAC last 2 bytes>) and set it active.
 */

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "openthread/dataset.h"
#include "openthread/dataset_ftd.h"
#include "openthread/instance.h"

#include "openthread/dataset_init.h"

#define TAG "dataset_init"

void dataset_init_on_boot(void)
{
    otInstance *instance = esp_openthread_get_instance();
    if (instance == NULL) {
        ESP_LOGW(TAG, "instance NULL, skip dataset check");
        return;
    }

    if (!esp_openthread_lock_acquire(pdMS_TO_TICKS(2000))) {
        ESP_LOGW(TAG, "lock timeout, skip dataset check");
        return;
    }

    otOperationalDatasetTlvs tlvs;
    otError err = otDatasetGetActiveTlvs(instance, &tlvs);

    if (err == OT_ERROR_NONE && tlvs.mLength > 0 && tlvs.mLength <= sizeof(tlvs.mTlvs)) {
        esp_openthread_lock_release();
        ESP_LOGI(TAG, "active dataset present (len=%u), skip init", (unsigned)tlvs.mLength);
        return;
    }

    /* No existing dataset -> create a random one (FTD only). */
    ESP_LOGW(TAG, "no valid active dataset (err=%d), creating new network", err);

    otOperationalDataset new_dataset;
    err = otDatasetCreateNewNetwork(instance, &new_dataset);
    if (err != OT_ERROR_NONE) {
        esp_openthread_lock_release();
        ESP_LOGE(TAG, "otDatasetCreateNewNetwork failed %d", err);
        return;
    }

    /* Set the network name to ESP-BR-<MAC last 2 bytes>. */
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_BASE) == ESP_OK) {
        char network_name[OT_NETWORK_NAME_MAX_SIZE + 1];
        snprintf(network_name, sizeof(network_name), "ESP-BR-%02X%02X", mac[4], mac[5]);
        size_t len = strlen(network_name);
        memcpy(new_dataset.mNetworkName.m8, network_name, len + 1);
        new_dataset.mComponents.mIsNetworkNamePresent = true;
    }

    err = otDatasetSetActive(instance, &new_dataset);
    if (err != OT_ERROR_NONE) {
        esp_openthread_lock_release();
        ESP_LOGE(TAG, "otDatasetSetActive failed %d", err);
        return;
    }
    ESP_LOGI(TAG, "created new random Thread dataset and set as active");

    esp_openthread_lock_release();
}
