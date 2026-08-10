/*
 * OT launch: start OpenThread, then boost leader weight and set the preferred
 * partition id so this device is always the Leader.
 */

#include <string.h>

#include "esp_log.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_ot_cli_extension.h"
#include "openthread/instance.h"
#include "openthread/thread.h"
#include "openthread/thread_ftd.h"

#include "console/console.h"
#include "openthread/ot_launch.h"

#define TAG "ot_launch"

void ot_launch_start(const esp_openthread_config_t *config)
{
#if CONFIG_OPENTHREAD_CLI
    console_start();
#endif

    ESP_ERROR_CHECK(esp_openthread_start(config));

    /* Highest leader weight to make this device always the Leader. */
    otInstance *instance = esp_openthread_get_instance();
    if (instance && esp_openthread_lock_acquire(pdMS_TO_TICKS(1000))) {
        otDeviceProperties device_props;
        const otDeviceProperties *current = otThreadGetDeviceProperties(instance);
        if (current) {
            memcpy(&device_props, current, sizeof(device_props));
        } else {
            memset(&device_props, 0, sizeof(device_props));
        }

        /* Leader weight adjustment max (+16). */
        device_props.mLeaderWeightAdjustment = 16;
        /* Border Router to increase the weight. */
        device_props.mIsBorderRouter = true;
        /* Stable external power supply to increase the weight. */
        device_props.mPowerSupply = OT_POWER_SUPPLY_EXTERNAL_STABLE;
        /* Stable (not unstable). */
        device_props.mIsUnstable = false;

        otThreadSetDeviceProperties(instance, &device_props);

        /* Highest preferred Leader Partition Id (0xFFFFFFFF). */
        otThreadSetPreferredLeaderPartitionId(instance, 0xFFFFFFFF);

        esp_openthread_lock_release();
        ESP_LOGI(TAG, "leader weight set to maximum (adjustment=+16, BR=true, stable)");
    } else {
        ESP_LOGW(TAG, "failed to set leader weight (instance NULL or lock timeout)");
    }

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
    esp_cli_custom_command_init();
#endif
    console_register_system();
}
