/*
 * OpenThread table snapshots (router/child/joiner) serialized into byte buffers.
 *
 * These helpers do NOT take the OpenThread lock. Caller must hold the lock
 * (e.g. via esp_openthread_lock_acquire()) when calling.
 */

#ifndef OPENTHREAD_OT_TABLE_SNAPSHOT_H
#define OPENTHREAD_OT_TABLE_SNAPSHOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "openthread/instance.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Build a Router Table snapshot.
 *
 * Format (same as frame protocol):
 *   - count(1)
 *   - repeated entries (15 bytes each):
 *       RouterId(1) + RLOC16(2) + ExtAddress(8) + LinkQualityIn(1) +
 *       LinkQualityOut(1) + Age(2)
 */
bool ot_table_snapshot_build_router_table(otInstance *instance, uint8_t *buf, size_t buf_size, size_t *out_len);

/*
 * Build a Child Table snapshot.
 *
 * Format (same as frame protocol):
 *   - count(1)
 *   - repeated entries (17 bytes each):
 *       ChildId(1) + RLOC16(2) + ExtAddress(8) + LinkQualityIn(1) +
 *       AverageRssi(1) + FullThreadDevice(1) + RxOnWhenIdle(1) + Age(2)
 */
bool ot_table_snapshot_build_child_table(otInstance *instance, uint8_t *buf, size_t buf_size, size_t *out_len);

/*
 * Build a Commissioner Joiner Table snapshot.
 *
 * Format (same as frame protocol):
 *   - count(1)
 *   - repeated entries (variable):
 *       Type(1) + SharedId(variable) + PSKD_length(1) + PSKD(variable) + ExpirationTime(4)
 */
bool ot_table_snapshot_build_joiner_table(otInstance *instance, uint8_t *buf, size_t buf_size, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* OPENTHREAD_OT_TABLE_SNAPSHOT_H */
