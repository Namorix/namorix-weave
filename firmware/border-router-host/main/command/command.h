/*
 * Command dispatch: bảng handler cho mọi CMD (STATE, DATASET_ACTIVE, IP_ADDR,
 * SET_*, TABLE, THREAD_*, RESET, FACTORY, COMMISSIONER_JOINER, SRP_REGISTER).
 * Plan 05: handler là stub NACK not-ready; Plan 06 thay bằng implement thật.
 */

#ifndef COMMAND_H
#define COMMAND_H

#include <stdint.h>

#include "esp_err.h"

#include "frame/frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Handler đồng bộ: trả 0 nếu đã gửi response, khác 0 nếu lỗi. */
typedef int (*command_handler_t)(frame_t *frame);

typedef struct {
    uint8_t cmd;
    command_handler_t handler;
} command_handler_entry_t;

/*
 * Dispatch frame tới handler trong bảng. CMD không có handler → gửi NACK
 * invalid-cmd. Gọi đồng bộ trong RX task (frame.data chỉ hợp lệ trong lúc gọi).
 */
esp_err_t command_dispatch(const frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_H */
