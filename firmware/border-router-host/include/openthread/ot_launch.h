/*
 * OT launch: start OpenThread and apply the leader weight / preferred partition boost.
 */

#ifndef OPENTHREAD_OT_LAUNCH_H
#define OPENTHREAD_OT_LAUNCH_H

#include "esp_openthread_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Start OpenThread, then boost leader weight (+16, BR, stable power) and set the preferred partition id. */
void ot_launch_start(const esp_openthread_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* OPENTHREAD_OT_LAUNCH_H */
