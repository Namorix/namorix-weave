/*
 * Frame TCP transport (public API): listen port, accept 1 client, RX loop +
 * dispatch, state watchdog.
 */

#ifndef FRAME_TCP_H
#define FRAME_TCP_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize transport: create mutexes, accept task (listen+accept 1 client), RX task, state watchdog. */
esp_err_t frame_tcp_init(void);

/* Mark that CMD_STATE was received from the backend (reset the state watchdog miss counter). */
void frame_tcp_state_mark_received(void);

/*
 * Mark that an IP_ADDR response was just sent for frame_id: arm the ACK confirm
 * timer. If the backend does not ACK within IP_RETRY_INTERVAL_MS, the response is
 * re-sent (via command_ipaddr_response) up to IP_RETRY_MAX times, then dropped.
 * Called by the CMD_IP_ADDR handler after it sends the ACK + RLOC payload.
 */
void frame_tcp_mark_ip_response_pending(uint8_t frame_id);

#ifdef __cplusplus
}
#endif

#endif /* FRAME_TCP_H */
