/*
 * OpenThread change detector: event-driven snapshot + diff.
 *
 * This module observes OpenThread state changes via otSetStateChangedCallback(),
 * debounces bursts, then rebuilds snapshots (role/rloc/dataset/tables) under the
 * OpenThread lock and computes a changed bitmask. On any change it pushes a
 * CMD_NOTIFY frame (payload = changed bitmask, u32 big-endian) to the backend.
 */

#ifndef OPENTHREAD_OT_CHANGE_DETECTOR_H
#define OPENTHREAD_OT_CHANGE_DETECTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "openthread/instance.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OT_CHANGED_MASK_ROLE        = (1u << 0),
    OT_CHANGED_MASK_IP          = (1u << 1),
    OT_CHANGED_MASK_DATASET     = (1u << 2),
    OT_CHANGED_MASK_ROUTER_TBL  = (1u << 3),
    OT_CHANGED_MASK_CHILD_TBL   = (1u << 4),
    OT_CHANGED_MASK_JOINER_TBL  = (1u << 5),
} ot_change_mask_t;

/*
 * Initialize the detector and register the OT state changed callback.
 *
 * - Safe to call once after OpenThread is started and the instance exists.
 * - Returns false on allocation/task/timer failures.
 */
bool ot_change_detector_init(otInstance *instance);

#ifdef __cplusplus
}
#endif

#endif /* OPENTHREAD_OT_CHANGE_DETECTOR_H */
