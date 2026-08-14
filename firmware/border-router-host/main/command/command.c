/*
 * Command handlers (Plan 06): real implementations replacing the NACK-not-ready
 * stubs. Shared helpers: scoped OT lock, dataset field mutator (5x set_*),
 * generic table reader (reuses ot_table_snapshot), deferred reset/factory
 * timers, IP_ADDR handshake (retry owned by the TCP transport), SRP register.
 */

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "openthread/commissioner.h"
#include "openthread/dataset.h"
#include "openthread/instance.h"
#include "openthread/ip6.h"
#include "openthread/link.h"
#include "openthread/srp_client.h"
#include "openthread/thread.h"
#include "sdkconfig.h"

#include "br_config.h"
#include "command/command.h"
#include "frame/frame.h"
#include "transport/frame_tcp.h"
#include "openthread/device_role.h"
#include "openthread/ot_table_snapshot.h"

#define TAG "command"

#define OT_LOCK_TIMEOUT_MS 1000
#define LEADER_RLOC_LEN    16

/* Deferred reset/factory: ACK first, execute after 2s so the backend gets the ACK. */
#define CMD_EXEC_DELAY_US (2000000ULL)

/* CMD_FACTORY is destructive: backend must attach this confirm byte, else NACK. */
#define CMD_FACTORY_CONFIRM_BYTE 0xAA

/* SRP client: 1 service `_dashboard._udp` so the backend can be discovered. */
#define SRP_HOSTNAME_MAX_LEN 63

/* BR_HEALTH TLV suffix types (after the 16-byte prefix). */
#define BR_HEALTH_PREFIX_SIZE     16
#define BR_HEALTH_TLV_BUF_SIZE    400
#define BR_HEALTH_TLV_TASK_NAME   0x01
#define BR_HEALTH_TLV_HIGH_WATER  0x02
#define BR_HEALTH_TLV_STACK_SIZE  0x03

static int send_ack(const frame_t *frame, const uint8_t *data, size_t len)
{
    return (frame_send(frame->frame_id, CMD_ACK, data, len) == ESP_OK) ? 0 : -1;
}

static void send_nack(const frame_t *frame, uint8_t code)
{
    (void)frame_send(frame->frame_id, CMD_NACK, &code, 1);
}

static bool ot_get_and_lock(otInstance **out)
{
    *out = esp_openthread_get_instance();
    if (*out == NULL)
        return false;
    return esp_openthread_lock_acquire(pdMS_TO_TICKS(OT_LOCK_TIMEOUT_MS));
}

static bool ot_lock_or_nack(const frame_t *frame, otInstance **out)
{
    if (!ot_get_and_lock(out)) {
        send_nack(frame, (*out == NULL) ? FRAME_NACK_NOT_READY : FRAME_NACK_TIMEOUT);
        return false;
    }
    return true;
}

static esp_timer_handle_t s_reset_timer   = NULL;
static esp_timer_handle_t s_factory_timer = NULL;

/* Shared by CMD_THREAD_STOP and the pre-reset shutdown. */
static void thread_graceful_shutdown(void)
{
    otInstance *i;
    if (!ot_get_and_lock(&i)) {
        ESP_LOGW(TAG, "graceful shutdown: lock timeout, skipping");
        return;
    }

    (void)otThreadSetEnabled(i, false);
    (void)otIp6SetEnabled(i, false);
    esp_openthread_lock_release();
    ESP_LOGI(TAG, "graceful shutdown: thread stopped, ip6 down");
}

/*
 * Shared factory reset: raw NVS erase + restart. Also called by the boot button
 * long-press (main.c). Does NOT stop OpenThread first — avoids an OT write-back
 * of the dataset into NVS after the erase; esp_restart() cuts everything off.
 */
void command_factory_reset(void)
{
    nvs_flash_deinit();
    const esp_partition_t *nvs_part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "nvs");
    if (nvs_part != NULL) {
        esp_err_t err = esp_partition_erase_range(nvs_part, 0, nvs_part->size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "partition erase failed: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(TAG, "NVS partition not found, fallback nvs_flash_erase");
        nvs_flash_erase();
    }

    esp_restart();
}

static void reset_timer_cb(void *arg)
{
    (void)arg;
    thread_graceful_shutdown();
    esp_restart();
}

static void factory_timer_cb(void *arg)
{
    (void)arg;
    command_factory_reset();
}

static esp_err_t start_deferred_timer(esp_timer_handle_t *handle, esp_timer_cb_t cb, const char *name)
{
    if (*handle == NULL) {
        const esp_timer_create_args_t args = {
            .callback        = cb,
            .arg             = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name            = name,
        };
        if (esp_timer_create(&args, handle) != ESP_OK) {
            return ESP_FAIL;
        }
    }

    esp_timer_stop(*handle); /* safe if the timer is not running */
    return esp_timer_start_once(*handle, CMD_EXEC_DELAY_US);
}

static uint8_t role_to_byte(otDeviceRole role)
{
    switch (role) {
        case OT_DEVICE_ROLE_DISABLED: return (uint8_t)DEVICE_ROLE_DISABLED;
        case OT_DEVICE_ROLE_DETACHED: return (uint8_t)DEVICE_ROLE_DETACHED;
        case OT_DEVICE_ROLE_CHILD:    return (uint8_t)DEVICE_ROLE_CHILD;
        case OT_DEVICE_ROLE_ROUTER:   return (uint8_t)DEVICE_ROLE_ROUTER;
        case OT_DEVICE_ROLE_LEADER:   return (uint8_t)DEVICE_ROLE_LEADER;
        default:                      return (uint8_t)DEVICE_ROLE_DISABLED;
    }
}

static int command_handle_state(const frame_t *frame)
{
    frame_tcp_state_mark_received();
    uint8_t role_byte = (uint8_t)DEVICE_ROLE_DISABLED;
    otInstance *i;
    if (ot_get_and_lock(&i)) {
        role_byte = role_to_byte(otThreadGetDeviceRole(i));
        esp_openthread_lock_release();
    }

    return send_ack(frame, &role_byte, 1);
}

static int command_handle_dataset_active(const frame_t *frame)
{
    otInstance *i;
    if (!ot_lock_or_nack(frame, &i)) {
        return -1;
    }

    otOperationalDatasetTlvs tlvs;
    otError err = otDatasetGetActiveTlvs(i, &tlvs);
    esp_openthread_lock_release();

    if (err != OT_ERROR_NONE || tlvs.mLength == 0 || tlvs.mLength > sizeof(tlvs.mTlvs)) {
        ESP_LOGE(TAG, "otDatasetGetActiveTlvs failed %d", (int)err);
        send_nack(frame, FRAME_NACK_NOT_READY);
        return -1;
    }

    return send_ack(frame, tlvs.mTlvs, (size_t)tlvs.mLength);
}

static uint8_t s_cached_leader_rloc[LEADER_RLOC_LEN];
static bool   s_cached_leader_rloc_valid = false;

static void refresh_leader_rloc_cache(void)
{
    otInstance *i;
    if (!ot_get_and_lock(&i))
        return;

    otIp6Address addr;
    if (otThreadGetLeaderRloc(i, &addr) == OT_ERROR_NONE) {
        memcpy(s_cached_leader_rloc, addr.mFields.m8, LEADER_RLOC_LEN);
        s_cached_leader_rloc_valid = true;
    }

    esp_openthread_lock_release();
}

/* Re-send the IP_ADDR response (used by the transport's ACK retry timer). */
int command_ipaddr_response(uint8_t frame_id)
{
    if (!s_cached_leader_rloc_valid)
        refresh_leader_rloc_cache();

    if (!s_cached_leader_rloc_valid) {
        uint8_t nack = FRAME_NACK_NOT_READY;
        (void)frame_send(frame_id, CMD_NACK, &nack, 1);
        return -1;
    }

    return (frame_send(frame_id, CMD_ACK, s_cached_leader_rloc, LEADER_RLOC_LEN) == ESP_OK) ? 0 : -1;
}

static int command_handle_ipaddr(const frame_t *frame)
{
    int rc = command_ipaddr_response(frame->frame_id);
    if (rc == 0)
        frame_tcp_mark_ip_response_pending(frame->frame_id);
    return rc;
}

static int command_handle_mac_address(const frame_t *frame)
{
    otInstance *i;
    if (ot_get_and_lock(&i)) {
        uint8_t mac[8];
        otExtAddress eui64;
        otLinkGetFactoryAssignedIeeeEui64(i, &eui64); /* stable per RCP hardware */
        memcpy(mac, eui64.m8, sizeof(mac));
        esp_openthread_lock_release();
        ESP_LOGI(TAG, "MAC source=ot_factory_ieee_eui64 %02x%02x%02x%02x%02x%02x%02x%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], mac[6], mac[7]);
        return send_ack(frame, mac, sizeof(mac));
    }

    if (i != NULL) {
        send_nack(frame, FRAME_NACK_TIMEOUT);
        return -1;
    }

    /* Last resort when the OT instance is not up: eFuse IEEE802154 EUI-64. */
    uint8_t mac[8];
    if (esp_read_mac(mac, ESP_MAC_IEEE802154) != ESP_OK) {
        ESP_LOGE(TAG, "esp_read_mac IEEE802154 failed");
        send_nack(frame, FRAME_NACK_NOT_READY);
        return -1;
    }

    return send_ack(frame, mac, sizeof(mac));
}

static size_t tlv_put(uint8_t *buf, size_t cap, size_t *off, uint8_t type, const void *val, size_t len)
{
    if (len > 255 || *off + 2 + len > cap)
        return 0;

    buf[(*off)++] = type;
    buf[(*off)++] = (uint8_t)len;
    memcpy(buf + *off, val, len);
    *off += len;
    return 2 + len;
}

static size_t tlv_put_u32_be(uint8_t *buf, size_t cap, size_t *off, uint8_t type, uint32_t v)
{
    uint8_t be[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
    return tlv_put(buf, cap, off, type, be, 4);
}

static int command_handle_br_health(const frame_t *frame)
{
    uint8_t payload[BR_HEALTH_PREFIX_SIZE + BR_HEALTH_TLV_BUF_SIZE];
    uint32_t free_heap = (uint32_t)esp_get_free_heap_size();
    uint32_t min_free  = (uint32_t)esp_get_minimum_free_heap_size();
    uint32_t uptime_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    uint32_t mle_detach = 0;
    otInstance *i;

    if (ot_get_and_lock(&i)) {
        const otMleCounters *counters = otThreadGetMleCounters(i);
        if (counters != NULL) {
            mle_detach = (uint32_t)counters->mDetachedRole;
        }
        esp_openthread_lock_release();
    }

    uint8_t *p = payload;
    *p++ = (uint8_t)(free_heap >> 24); *p++ = (uint8_t)(free_heap >> 16);
    *p++ = (uint8_t)(free_heap >> 8);  *p++ = (uint8_t)free_heap;
    *p++ = (uint8_t)(min_free >> 24);  *p++ = (uint8_t)(min_free >> 16);
    *p++ = (uint8_t)(min_free >> 8);   *p++ = (uint8_t)min_free;
    *p++ = (uint8_t)(uptime_ms >> 24); *p++ = (uint8_t)(uptime_ms >> 16);
    *p++ = (uint8_t)(uptime_ms >> 8);  *p++ = (uint8_t)uptime_ms;
    *p++ = (uint8_t)(mle_detach >> 24);*p++ = (uint8_t)(mle_detach >> 16);
    *p++ = (uint8_t)(mle_detach >> 8); *p++ = (uint8_t)mle_detach;

    size_t off = 0;
    for (size_t i = 0; i < k_br_tasks_count; i++) {
        const char *name = k_br_tasks[i].name;
        size_t name_len = strlen(name);
        TaskHandle_t h = xTaskGetHandle(name);
        uint32_t hwm_bytes = 0;
        if (h != NULL) {
            UBaseType_t hwm_words = uxTaskGetStackHighWaterMark(h);
            hwm_bytes = (uint32_t)hwm_words * (uint32_t)sizeof(StackType_t);
        }

        if (!tlv_put(payload, BR_HEALTH_TLV_BUF_SIZE, &off, BR_HEALTH_TLV_TASK_NAME, name, name_len))
            return send_ack(frame, payload, BR_HEALTH_PREFIX_SIZE);

        if (!tlv_put_u32_be(payload, BR_HEALTH_TLV_BUF_SIZE, &off, BR_HEALTH_TLV_HIGH_WATER, hwm_bytes))
            return send_ack(frame, payload, BR_HEALTH_PREFIX_SIZE);

        if (!tlv_put_u32_be(payload, BR_HEALTH_TLV_BUF_SIZE, &off, BR_HEALTH_TLV_STACK_SIZE, k_br_tasks[i].stack))
            return send_ack(frame, payload, BR_HEALTH_PREFIX_SIZE);
    }

    return send_ack(frame, payload, BR_HEALTH_PREFIX_SIZE + off);
}

typedef bool (*table_builder_t)(otInstance *instance, uint8_t *buf, size_t buf_size, size_t *out_len);

static int handle_table_read(const frame_t *frame, table_builder_t builder)
{
    otInstance *i;
    if (!ot_lock_or_nack(frame, &i))
        return -1;

    uint8_t buf[512];
    size_t out_len = 0;
    bool ok = builder(i, buf, sizeof(buf), &out_len);
    esp_openthread_lock_release();

    if (!ok || out_len == 0) {
        send_nack(frame, FRAME_NACK_NOT_READY);
        return -1;
    }

    return send_ack(frame, buf, out_len);
}

static int command_handle_router_table(const frame_t *frame)
{
    return handle_table_read(frame, ot_table_snapshot_build_router_table);
}

static int command_handle_child_table(const frame_t *frame)
{
    return handle_table_read(frame, ot_table_snapshot_build_child_table);
}

static int command_handle_joiner_table(const frame_t *frame)
{
    return handle_table_read(frame, ot_table_snapshot_build_joiner_table);
}

typedef struct {
    uint8_t min_len;
    uint8_t max_len;
    otError (*validate)(const uint8_t *data, size_t len); /* NULL = no extra check */
    void    (*apply)(otOperationalDataset *ds, const uint8_t *data, size_t len);
} dataset_field_t;

static otError validate_panid(const uint8_t *data, size_t len)
{
    (void)len;
    uint16_t panid = ((uint16_t)data[0] << 8) | data[1];
    return (panid == 0xFFFF) ? OT_ERROR_INVALID_ARGS : OT_ERROR_NONE;
}

static void apply_panid(otOperationalDataset *ds, const uint8_t *data, size_t len)
{
    (void)len;
    ds->mPanId = ((uint16_t)data[0] << 8) | data[1];
    ds->mComponents.mIsPanIdPresent = true;
}

static otError validate_channel(const uint8_t *data, size_t len)
{
    (void)len;
    return (data[0] < 11 || data[0] > 26) ? OT_ERROR_INVALID_ARGS : OT_ERROR_NONE;
}

static void apply_channel(otOperationalDataset *ds, const uint8_t *data, size_t len)
{
    (void)len;
    ds->mChannel = data[0];
    ds->mComponents.mIsChannelPresent = true;
}

static void apply_network_name(otOperationalDataset *ds, const uint8_t *data, size_t len)
{
    size_t name_len = len;
    if (name_len > OT_NETWORK_NAME_MAX_SIZE - 1)
        name_len = OT_NETWORK_NAME_MAX_SIZE - 1;

    memcpy(ds->mNetworkName.m8, data, name_len);
    ds->mNetworkName.m8[name_len] = '\0';
    ds->mComponents.mIsNetworkNamePresent = true;
}

static void apply_extended_panid(otOperationalDataset *ds, const uint8_t *data, size_t len)
{
    memcpy(ds->mExtendedPanId.m8, data, len);
    ds->mComponents.mIsExtendedPanIdPresent = true;
}

static void apply_network_key(otOperationalDataset *ds, const uint8_t *data, size_t len)
{
    memcpy(ds->mNetworkKey.m8, data, len);
    ds->mComponents.mIsNetworkKeyPresent = true;
}

static const dataset_field_t FIELD_PANID        = { 2, 2, validate_panid, apply_panid };
static const dataset_field_t FIELD_CHANNEL      = { 1, 1, validate_channel, apply_channel };
static const dataset_field_t FIELD_NETWORK_NAME = { 1, OT_NETWORK_NAME_MAX_SIZE, NULL, apply_network_name };
static const dataset_field_t FIELD_EXTENDED_PANID = { 8, 8, NULL, apply_extended_panid };
static const dataset_field_t FIELD_NETWORK_KEY  = { OT_NETWORK_KEY_SIZE, OT_NETWORK_KEY_SIZE, NULL, apply_network_key };

static int handle_dataset_set(const frame_t *frame, const dataset_field_t *field)
{
    if (frame->data == NULL || frame->len < field->min_len || frame->len > field->max_len ||
        (field->validate != NULL && field->validate(frame->data, frame->len) != OT_ERROR_NONE)) {
        send_nack(frame, FRAME_NACK_INVALID_PARAM);
        return -1;
    }

    otInstance *i;
    if (!ot_lock_or_nack(frame, &i))
        return -1;


    otOperationalDataset ds;
    otError err = otDatasetGetActive(i, &ds);
    if (err != OT_ERROR_NONE) {
        esp_openthread_lock_release();
        ESP_LOGE(TAG, "otDatasetGetActive failed %d", (int)err);
        send_nack(frame, FRAME_NACK_NOT_READY);
        return -1;
    }

    field->apply(&ds, frame->data, frame->len);
    err = otDatasetSetActive(i, &ds);
    esp_openthread_lock_release();
    if (err != OT_ERROR_NONE) {
        ESP_LOGE(TAG, "otDatasetSetActive failed %d", (int)err);
        send_nack(frame, FRAME_NACK_NOT_READY);
        return -1;
    }

    return send_ack(frame, NULL, 0);
}

static int command_handle_set_panid(const frame_t *frame)
{
    return handle_dataset_set(frame, &FIELD_PANID);
}

static int command_handle_set_channel(const frame_t *frame)
{
    return handle_dataset_set(frame, &FIELD_CHANNEL);
}

static int command_handle_set_network_name(const frame_t *frame)
{
    return handle_dataset_set(frame, &FIELD_NETWORK_NAME);
}

static int command_handle_set_extended_panid(const frame_t *frame)
{
    return handle_dataset_set(frame, &FIELD_EXTENDED_PANID);
}

static int command_handle_set_network_key(const frame_t *frame)
{
    return handle_dataset_set(frame, &FIELD_NETWORK_KEY);
}

static int command_handle_thread_start(const frame_t *frame)
{
    otInstance *i;
    if (!ot_lock_or_nack(frame, &i))
        return -1;

    (void)otIp6SetEnabled(i, true);
    (void)otThreadSetEnabled(i, true);
    esp_openthread_lock_release();

    return send_ack(frame, NULL, 0);
}

static int command_handle_thread_stop(const frame_t *frame)
{
    otInstance *i;
    if (!ot_lock_or_nack(frame, &i))
        return -1;

    (void)otThreadSetEnabled(i, false);
    (void)otIp6SetEnabled(i, false);
    esp_openthread_lock_release();

    return send_ack(frame, NULL, 0);
}

static int command_handle_reset(const frame_t *frame)
{
    if (send_ack(frame, NULL, 0) != 0)
        return -1;

    if (start_deferred_timer(&s_reset_timer, reset_timer_cb, "cmd_reset") != ESP_OK) {
        ESP_LOGE(TAG, "CMD_RESET: timer failed, restarting immediately");
        esp_restart();
    }

    ESP_LOGW(TAG, "CMD_RESET: ACK sent, restarting in 2s");
    return 0;
}

static int command_handle_factory(const frame_t *frame)
{
    /* Destructive command: require the confirm byte, else NACK invalid-param. */
    if (frame->data == NULL || frame->len != 1 || frame->data[0] != CMD_FACTORY_CONFIRM_BYTE) {
        ESP_LOGW(TAG, "CMD_FACTORY: missing/invalid confirm byte");
        send_nack(frame, FRAME_NACK_INVALID_PARAM);
        return -1;
    }

    if (send_ack(frame, NULL, 0) != 0)
        return -1;

    if (start_deferred_timer(&s_factory_timer, factory_timer_cb, "cmd_factory") != ESP_OK) {
        ESP_LOGE(TAG, "CMD_FACTORY: timer failed, factory reset immediately");
        command_factory_reset();
    }

    ESP_LOGW(TAG, "CMD_FACTORY: ACK sent, factory reset in 2s");
    return 0;
}

static int command_handle_thread_version(const frame_t *frame)
{
    const char *version = otGetVersionString();
    if (version == NULL) {
        send_nack(frame, FRAME_NACK_NOT_READY);
        return -1;
    }

    size_t len = strlen(version);
    if (len > 64)
        len = 64;

    return send_ack(frame, (const uint8_t *)version, len);
}

static int command_handle_commissioner_joiner(const frame_t *frame)
{
    if (frame->data == NULL || frame->len < 14) {
        send_nack(frame, FRAME_NACK_INVALID_PARAM);
        return -1;
    }

    const uint8_t *p = frame->data;
    uint8_t eui64[8];
    memcpy(eui64, p, 8);
    p += 8;

    uint8_t pskd_len = *p++;
    if (pskd_len == 0 || pskd_len > OT_JOINER_MAX_PSKD_LENGTH) {
        send_nack(frame, FRAME_NACK_INVALID_PARAM);
        return -1;
    }

    if (frame->len != (size_t)(8 + 1 + pskd_len + 4)) {
        send_nack(frame, FRAME_NACK_INVALID_PARAM);
        return -1;
    }

    char pskd_str[OT_JOINER_MAX_PSKD_LENGTH + 1];
    memcpy(pskd_str, p, pskd_len);
    pskd_str[pskd_len] = '\0';
    p += pskd_len;

    uint32_t timeout_s = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
                       | ((uint32_t)p[2] << 8)  | (uint32_t)p[3];

    static const uint8_t k_zero_eui64[8] = { 0 };
    bool is_wildcard = (memcmp(eui64, k_zero_eui64, 8) == 0);

    otInstance *i;
    if (!ot_lock_or_nack(frame, &i))
        return -1;


    otCommissionerState comm_state = otCommissionerGetState(i);
    if (comm_state != OT_COMMISSIONER_STATE_ACTIVE) {
        otError start_err = otCommissionerStart(i, NULL, NULL, NULL);
        if (start_err != OT_ERROR_NONE && start_err != OT_ERROR_ALREADY) {
            esp_openthread_lock_release();
            ESP_LOGE(TAG, "otCommissionerStart failed %d", (int)start_err);
            send_nack(frame, FRAME_NACK_NOT_READY);
            return -1;
        }

        esp_openthread_lock_release();
        /* Wait for the commissioner to become ACTIVE; release the lock between
         * polls so the OT task can process the petition. */
        ESP_LOGI(TAG, "Commissioner: waiting for ACTIVE state...");
        const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(1000);
        bool became_active = false;

        while (xTaskGetTickCount() < deadline) {
            vTaskDelay(pdMS_TO_TICKS(200));
            if (!ot_get_and_lock(&i)) {
                continue;
            }

            comm_state = otCommissionerGetState(i);
            esp_openthread_lock_release();
            if (comm_state == OT_COMMISSIONER_STATE_ACTIVE) {
                became_active = true;
                break;
            }
        }

        if (!became_active) {
            ESP_LOGE(TAG, "Commissioner: timed out waiting for ACTIVE (state=%d)", (int)comm_state);
            send_nack(frame, FRAME_NACK_NOT_READY);
            return -1;
        }

        if (!ot_lock_or_nack(frame, &i))
            return -1;
    }
    otExtAddress ot_eui64;
    memcpy(ot_eui64.m8, eui64, 8);
    otError err = otCommissionerAddJoiner(i, is_wildcard ? NULL : &ot_eui64, pskd_str, timeout_s);
    esp_openthread_lock_release();
    if (err != OT_ERROR_NONE) {
        ESP_LOGE(TAG, "otCommissionerAddJoiner failed %d (pskd=%s, timeout=%lu, wildcard=%d)",
                 (int)err, pskd_str, (unsigned long)timeout_s, (int)is_wildcard);
        send_nack(frame, (err == OT_ERROR_INVALID_ARGS) ? FRAME_NACK_INVALID_PARAM : FRAME_NACK_NOT_READY);
        return -1;
    }

    ESP_LOGI(TAG, "Commissioner: joiner added pskd=%s timeout=%lus wildcard=%d",
             pskd_str, (unsigned long)timeout_s, (int)is_wildcard);
    return send_ack(frame, NULL, 0);
}

static otSrpClientService s_srp_dashboard_service;
static bool   s_srp_dashboard_service_inited = false;
/* OT SRP client stores hostname/address pointers (no copy) — static buffers to avoid dangling. */
static char          s_srp_hostname[SRP_HOSTNAME_MAX_LEN + 1];
static otIp6Address s_srp_backend_addr;

static int command_handle_srp_register(const frame_t *frame)
{
    if (frame->data == NULL || frame->len < (size_t)(1 + 16 + 2)) {
        send_nack(frame, FRAME_NACK_INVALID_PARAM);
        return -1;
    }

    const uint8_t *p = frame->data;
    uint8_t hostname_len = *p++;
    if (hostname_len == 0 || hostname_len > SRP_HOSTNAME_MAX_LEN) {
        send_nack(frame, FRAME_NACK_INVALID_PARAM);
        return -1;
    }

    if (frame->len != (size_t)(1 + hostname_len + 16 + 2)) {
        send_nack(frame, FRAME_NACK_INVALID_PARAM);
        return -1;
    }

    char hostname[SRP_HOSTNAME_MAX_LEN + 1];
    memcpy(hostname, p, hostname_len);
    hostname[hostname_len] = '\0';
    p += hostname_len;

    if (hostname[0] == '\0') {
        ESP_LOGW(TAG, "SRP: hostname empty, reject");
        send_nack(frame, FRAME_NACK_INVALID_PARAM);
        return -1;
    }

    memcpy(s_srp_hostname, hostname, (size_t)hostname_len + 1);
    memcpy(&s_srp_backend_addr, p, sizeof(s_srp_backend_addr));
    p += sizeof(s_srp_backend_addr);

    uint16_t port = ((uint16_t)p[0] << 8) | (uint16_t)p[1];
    if (port == 0) {
        send_nack(frame, FRAME_NACK_INVALID_PARAM);
        return -1;
    }

    otInstance *i;
    if (!ot_lock_or_nack(frame, &i)) {
        return -1;
    }
    /* Auto-start so the SRP client finds the SRP server in the mesh. */
    (void)otSrpClientEnableAutoStartMode(i, NULL, NULL);
    (void)otSrpClientClearHostAndServices(i);

    otError err = otSrpClientSetHostName(i, s_srp_hostname);
    if (err != OT_ERROR_NONE) {
        esp_openthread_lock_release();
        ESP_LOGE(TAG, "SRP: otSrpClientSetHostName failed %d", (int)err);
        send_nack(frame, FRAME_NACK_NOT_READY);
        return -1;
    }

    err = otSrpClientSetHostAddresses(i, &s_srp_backend_addr, 1);
    if (err != OT_ERROR_NONE) {
        esp_openthread_lock_release();
        ESP_LOGE(TAG, "SRP: otSrpClientSetHostAddresses failed %d", (int)err);
        send_nack(frame, FRAME_NACK_NOT_READY);
        return -1;
    }

    if (!s_srp_dashboard_service_inited) {
        memset(&s_srp_dashboard_service, 0, sizeof(s_srp_dashboard_service));
        s_srp_dashboard_service.mName           = "_dashboard._udp";
        s_srp_dashboard_service.mInstanceName   = "dashboard";
        s_srp_dashboard_service.mPriority       = 0;
        s_srp_dashboard_service.mWeight         = 0;
        s_srp_dashboard_service.mSubTypeLabels  = NULL;
        s_srp_dashboard_service.mTxtEntries     = NULL;
        s_srp_dashboard_service.mNumTxtEntries  = 0;
        s_srp_dashboard_service.mLease          = 10;
        s_srp_dashboard_service.mKeyLease       = 120;
        s_srp_dashboard_service_inited = true;
    }
    s_srp_dashboard_service.mPort = port;

    err = otSrpClientAddService(i, &s_srp_dashboard_service);
    esp_openthread_lock_release();
    if (err != OT_ERROR_NONE) {
        ESP_LOGE(TAG, "SRP: otSrpClientAddService failed %d", (int)err);
        send_nack(frame, FRAME_NACK_NOT_READY);
        return -1;
    }
    /* Auto-start already submits the SRP Update; never call otSrpClientStart(NULL)
     * — that API dereferences the server address and crashes. */
    ESP_LOGI(TAG, "SRP register OK: _dashboard._udp -> %s port %u", hostname, (unsigned)port);
    return send_ack(frame, NULL, 0);
}

static const command_handler_entry_t s_handlers[] = {
    { CMD_STATE, command_handle_state },
    { CMD_RESET, command_handle_reset },
    { CMD_FACTORY, command_handle_factory },
    { CMD_IP_ADDR, command_handle_ipaddr },
    { CMD_DATASET_ACTIVE, command_handle_dataset_active },
    { CMD_MAC_ADDRESS, command_handle_mac_address },
    { CMD_BR_HEALTH, command_handle_br_health },
    { CMD_ROUTER_TABLE, command_handle_router_table },
    { CMD_CHILD_TABLE, command_handle_child_table },
    { CMD_JOINER_TABLE, command_handle_joiner_table },
    { CMD_SET_PANID, command_handle_set_panid },
    { CMD_SET_CHANNEL, command_handle_set_channel },
    { CMD_SET_NETWORK_NAME, command_handle_set_network_name },
    { CMD_SET_EXTENDED_PANID, command_handle_set_extended_panid },
    { CMD_SET_NETWORK_KEY, command_handle_set_network_key },
    { CMD_THREAD_START, command_handle_thread_start },
    { CMD_THREAD_STOP, command_handle_thread_stop },
    { CMD_THREAD_VERSION, command_handle_thread_version },
    { CMD_COMMISSIONER_JOINER, command_handle_commissioner_joiner },
    { CMD_SRP_REGISTER, command_handle_srp_register },
};

esp_err_t command_dispatch(const frame_t *frame)
{
    for (size_t i = 0; i < sizeof(s_handlers) / sizeof(s_handlers[0]); i++) {
        if (s_handlers[i].cmd == frame->cmd) {
            int rc = s_handlers[i].handler(frame);
            if (rc != 0)
                ESP_LOGW(TAG, "%s handle failed rc=%d", frame_cmd_name(frame->cmd), rc);
            return (rc == 0) ? ESP_OK : ESP_FAIL;
        }
    }

    ESP_LOGW(TAG, "unknown cmd 0x%02x frame_id=%u", frame->cmd, (unsigned)frame->frame_id);
    uint8_t nack = FRAME_NACK_INVALID_CMD;
    (void)frame_send(frame->frame_id, CMD_NACK, &nack, 1);
    return ESP_OK;
}
