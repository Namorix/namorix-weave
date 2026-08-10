/*
 * Command dispatch: handler table for every CMD (STATE, DATASET_ACTIVE, IP_ADDR,
 * SET_*, TABLE, THREAD_*, RESET, FACTORY, COMMISSIONER_JOINER, SRP_REGISTER).
 */

#ifndef COMMAND_H
#define COMMAND_H

#include <stdint.h>

#include "esp_err.h"

#include "frame/frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Synchronous handler: returns 0 if a response was sent, non-zero on error. */
typedef int (*command_handler_t)(const frame_t *frame);

typedef struct {
    uint8_t cmd;
    command_handler_t handler;
} command_handler_entry_t;

/*
 * Dispatch a frame to the handler in the table. CMD with no handler -> send NACK
 * invalid-cmd. Called synchronously in the RX task (frame.data is valid only during the call).
 */
esp_err_t command_dispatch(const frame_t *frame);

/*
 * Raw factory reset: erase the NVS partition then restart. Does not stop the
 * Thread stack first. Shared by the boot button and CMD_FACTORY.
 */
void command_factory_reset(void);

/*
 * Re-send the cached CMD_IP_ADDR response (leader RLOC). Called by the transport
 * ACK retry timer when the backend did not confirm the handshake.
 */
int command_ipaddr_response(uint8_t frame_id);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_H */
