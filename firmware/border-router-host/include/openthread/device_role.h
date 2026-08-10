/*
 * Device role (OpenThread): 1-byte value sent in CMD_STATE (BR -> backend).
 * Mirror this enum to the backend for shared use.
 */

#ifndef OPENTHREAD_DEVICE_ROLE_H
#define OPENTHREAD_DEVICE_ROLE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DEVICE_ROLE_DISABLED = 0,
    DEVICE_ROLE_DETACHED = 1,
    DEVICE_ROLE_CHILD    = 2,
    DEVICE_ROLE_ROUTER   = 3,
    DEVICE_ROLE_LEADER   = 4,
} device_role_t;

#ifdef __cplusplus
}
#endif

#endif /* OPENTHREAD_DEVICE_ROLE_H */
