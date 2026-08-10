/*
 * OpenThread change detector: event-driven snapshot + diff + CMD_NOTIFY push.
 */

#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "esp_openthread_lock.h"
#include "esp_timer.h"
#include "freertos/task.h"

#include "frame/frame.h"
#include "openthread/dataset.h"
#include "openthread/ip6.h"
#include "openthread/thread.h"
#include "openthread/ot_table_snapshot.h"

#include "br_config.h"
#include "openthread/ot_change_detector.h"

#define TAG "ot_change"

#define DEBOUNCE_MS 250

/* Snapshot buffer sizes match the current frame protocol packers. */
#define SNAPSHOT_ROUTER_BUF  256
#define SNAPSHOT_CHILD_BUF   512
#define SNAPSHOT_JOINER_BUF  512
#define SNAPSHOT_DATASET_BUF 256

typedef struct {
    uint8_t role;

    uint8_t leader_rloc[16];
    bool leader_rloc_valid;

    uint8_t dataset_tlvs[SNAPSHOT_DATASET_BUF];
    uint16_t dataset_len;

    uint8_t router_tbl[SNAPSHOT_ROUTER_BUF];
    uint16_t router_len;

    uint8_t child_tbl[SNAPSHOT_CHILD_BUF];
    uint16_t child_len;

    uint8_t joiner_tbl[SNAPSHOT_JOINER_BUF];
    uint16_t joiner_len;
} snapshots_t;

static otInstance *s_instance;
static esp_timer_handle_t s_debounce_timer;
static TaskHandle_t s_task;

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool s_debounce_armed;

static uint8_t s_notify_frame_id = 0x80;

static snapshots_t s_prev;
static bool s_prev_valid;

static bool snapshot_equal_blob(const uint8_t *a, uint16_t a_len, const uint8_t *b, uint16_t b_len)
{
    if (a_len != b_len) {
        return false;
    }
    if (a_len == 0) {
        return true;
    }
    return memcmp(a, b, a_len) == 0;
}

static bool build_snapshots_locked(snapshots_t *out)
{
    memset(out, 0, sizeof(*out));

    /* role */
    out->role = (uint8_t)otThreadGetDeviceRole(s_instance);

    /* leader rloc (used by the frame protocol) */
    otIp6Address addr;
    if (otThreadGetLeaderRloc(s_instance, &addr) == OT_ERROR_NONE) {
        memcpy(out->leader_rloc, addr.mFields.m8, sizeof(out->leader_rloc));
        out->leader_rloc_valid = true;
    } else {
        out->leader_rloc_valid = false;
    }

    /* active dataset tlvs */
    otOperationalDatasetTlvs tlvs;
    otError err = otDatasetGetActiveTlvs(s_instance, &tlvs);
    if (err == OT_ERROR_NONE && tlvs.mLength > 0) {
        uint16_t n = (uint16_t)tlvs.mLength;
        if (n > SNAPSHOT_DATASET_BUF) {
            n = SNAPSHOT_DATASET_BUF;
        }
        memcpy(out->dataset_tlvs, tlvs.mTlvs, n);
        out->dataset_len = n;
    } else {
        out->dataset_len = 0;
    }

    /* tables */
    size_t n = 0;
    if (ot_table_snapshot_build_router_table(s_instance, out->router_tbl, sizeof(out->router_tbl), &n)) {
        out->router_len = (uint16_t)n;
    }
    n = 0;
    if (ot_table_snapshot_build_child_table(s_instance, out->child_tbl, sizeof(out->child_tbl), &n)) {
        out->child_len = (uint16_t)n;
    }
    n = 0;
    if (ot_table_snapshot_build_joiner_table(s_instance, out->joiner_tbl, sizeof(out->joiner_tbl), &n)) {
        out->joiner_len = (uint16_t)n;
    }

    return true;
}

static void compute_diff_and_push(const snapshots_t *cur)
{
    uint32_t mask = 0;
    if (!s_prev_valid) {
        mask = OT_CHANGED_MASK_ROLE | OT_CHANGED_MASK_IP | OT_CHANGED_MASK_DATASET |
               OT_CHANGED_MASK_ROUTER_TBL | OT_CHANGED_MASK_CHILD_TBL | OT_CHANGED_MASK_JOINER_TBL;
    } else {
        if (cur->role != s_prev.role) {
            mask |= OT_CHANGED_MASK_ROLE;
        }

        if (cur->leader_rloc_valid != s_prev.leader_rloc_valid ||
            (cur->leader_rloc_valid && memcmp(cur->leader_rloc, s_prev.leader_rloc, sizeof(cur->leader_rloc)) != 0)) {
            mask |= OT_CHANGED_MASK_IP;
        }

        if (!snapshot_equal_blob(cur->dataset_tlvs, cur->dataset_len, s_prev.dataset_tlvs, s_prev.dataset_len)) {
            mask |= OT_CHANGED_MASK_DATASET;
        }
        if (!snapshot_equal_blob(cur->router_tbl, cur->router_len, s_prev.router_tbl, s_prev.router_len)) {
            mask |= OT_CHANGED_MASK_ROUTER_TBL;
        }
        if (!snapshot_equal_blob(cur->child_tbl, cur->child_len, s_prev.child_tbl, s_prev.child_len)) {
            mask |= OT_CHANGED_MASK_CHILD_TBL;
        }
        if (!snapshot_equal_blob(cur->joiner_tbl, cur->joiner_len, s_prev.joiner_tbl, s_prev.joiner_len)) {
            mask |= OT_CHANGED_MASK_JOINER_TBL;
        }
    }

    if (mask != 0) {
        ESP_LOGI(TAG, "changed mask=0x%08" PRIx32 " role=%u rloc=%s dataset_len=%u router_len=%u child_len=%u joiner_len=%u",
                 mask,
                 (unsigned)cur->role,
                 cur->leader_rloc_valid ? "yes" : "no",
                 (unsigned)cur->dataset_len,
                 (unsigned)cur->router_len,
                 (unsigned)cur->child_len,
                 (unsigned)cur->joiner_len);

        /* Push notify to the backend (fire-and-forget): payload = changed_mask (u32 BE). */
        uint8_t payload[4] = {
            (uint8_t)(mask >> 24),
            (uint8_t)(mask >> 16),
            (uint8_t)(mask >> 8),
            (uint8_t)(mask & 0xFF),
        };
        (void)frame_send(s_notify_frame_id++, CMD_NOTIFY, payload, sizeof(payload));
    }

    s_prev = *cur;
    s_prev_valid = true;
}

static void detector_task(void *pv)
{
    (void)pv;
    for (;;) {
        /* Wait until the debounce timer fires and notifies us. */
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!s_instance) {
            continue;
        }

        if (!esp_openthread_lock_acquire(pdMS_TO_TICKS(1000))) {
            ESP_LOGW(TAG, "lock timeout, skip snapshot");
            continue;
        }
        snapshots_t cur;
        (void)build_snapshots_locked(&cur);
        esp_openthread_lock_release();

        compute_diff_and_push(&cur);
    }
}

static void debounce_timer_cb(void *arg)
{
    (void)arg;
    portENTER_CRITICAL(&s_mux);
    s_debounce_armed = false;
    portEXIT_CRITICAL(&s_mux);
    if (s_task) {
        xTaskNotifyGive(s_task);
    }
}

static void on_ot_state_changed(otChangedFlags flags, void *context)
{
    (void)flags;
    (void)context;

    if (s_debounce_timer) {
        bool start = false;
        portENTER_CRITICAL(&s_mux);
        if (!s_debounce_armed) {
            s_debounce_armed = true;
            start = true;
        }
        portEXIT_CRITICAL(&s_mux);
        if (start) {
            (void)esp_timer_start_once(s_debounce_timer, (uint64_t)DEBOUNCE_MS * 1000ULL);
        }
    }
}

bool ot_change_detector_init(otInstance *instance)
{
    if (!instance || s_task != NULL) {
        return false;
    }
    s_instance = instance;
    s_prev_valid = false;
    s_debounce_armed = false;

    if (xTaskCreate(detector_task, TASK_NAME_OT_CHANGE, TASK_STACK_OT_CHANGE, NULL,
                    TASK_PRIO_OT_CHANGE, &s_task) != pdPASS) {
        s_task = NULL;
        return false;
    }

    const esp_timer_create_args_t args = {
        .callback = debounce_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ot_change_db",
    };
    if (esp_timer_create(&args, &s_debounce_timer) != ESP_OK) {
        vTaskDelete(s_task);
        s_task = NULL;
        return false;
    }

    (void)otSetStateChangedCallback(instance, on_ot_state_changed, NULL);
    ESP_LOGI(TAG, "init OK (debounce=%ums)", (unsigned)DEBOUNCE_MS);
    return true;
}
