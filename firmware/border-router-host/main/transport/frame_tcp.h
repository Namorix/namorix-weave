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

/* Khởi tạo transport: tạo mutex, accept task (listen+accept 1 client), RX task, state watchdog. */
esp_err_t frame_tcp_init(void);

/* Đánh dấu đã nhận CMD_STATE từ backend (reset miss counter của state watchdog). */
void frame_tcp_state_mark_received(void);

#ifdef __cplusplus
}
#endif

#endif /* FRAME_TCP_H */
