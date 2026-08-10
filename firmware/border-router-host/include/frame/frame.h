/*
 * Frame protocol: frame [SOF][FrameID][CMD][LEN_H][LEN_L][DATA][CRC8][EOF]
 * on the BR <-> backend (TCP) channel. Wire format per docs/usb_cdc_frame_structure.md:
 * CRC-8/MAXIM, LEN big-endian, no escaping.
 */

#ifndef FRAME_H
#define FRAME_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CMD theo docs/usb_cdc_frame_structure.md. 0x05–0x0F, 0x15–0x1F, 0x25–0x2F, 0x33–0x3F reserved. */
#define CMD_DATA                   0x01
#define CMD_ACK                    0x02
#define CMD_NACK                   0x03

#define CMD_RESET                  0x10
#define CMD_FACTORY                0x11
#define CMD_STATE                  0x12
#define CMD_IP_ADDR                0x13
#define CMD_DATASET_ACTIVE         0x14
#define CMD_DATASET_COMMIT_ACTIVE  0x15
#define CMD_MAC_ADDRESS            0x16
#define CMD_BR_HEALTH              0x17

/* Set config commands (0x20–0x2F) */
#define CMD_SET_PANID          0x20
#define CMD_SET_CHANNEL        0x21
#define CMD_SET_NETWORK_NAME   0x22
#define CMD_SET_EXTENDED_PANID 0x23
#define CMD_SET_NETWORK_KEY    0x24

/* Table commands (0x30–0x3F) */
#define CMD_ROUTER_TABLE    0x30
#define CMD_CHILD_TABLE     0x31
#define CMD_JOINER_TABLE    0x32

/* Thread start/stop/version, commissioner_joiner, SRP register */
#define CMD_THREAD_START        0x40
#define CMD_THREAD_STOP         0x41
#define CMD_THREAD_VERSION      0x42
#define CMD_COMMISSIONER_JOINER 0x43
#define CMD_SRP_REGISTER        0x44

/* Notify (BR -> backend): change bitmask */
#define CMD_NOTIFY              0x45

/* NACK codes — shared by firmware + backend (see docs). */
typedef enum {
    FRAME_NACK_INVALID_CMD   = 0x01,
    FRAME_NACK_NOT_READY     = 0x02,
    FRAME_NACK_TIMEOUT       = 0x03,
    FRAME_NACK_INVALID_PARAM = 0x04,
    FRAME_NACK_BUSY          = 0x05,
} frame_nack_t;

/* Wire constants */
#define FRAME_SOF              0xAA
#define FRAME_EOF              0x55
#define FRAME_HEADER_LEN       4      /* FrameID + CMD + LEN_H + LEN_L */
#define FRAME_FIXED_OVERHEAD   7      /* SOF + header + CRC + EOF (LEN=0) */
#define FRAME_MAX_DATA_LEN     2048
#define FRAME_MAX_BUFFER       (FRAME_FIXED_OVERHEAD + FRAME_MAX_DATA_LEN)

/*
 * Parsed frame. data points into the transport RX buffer — valid only during
 * dispatch (handler must process synchronously, must not keep the pointer).
 */
typedef struct {
    uint8_t frame_id;
    uint8_t cmd;
    uint16_t len;
    const uint8_t *data;
} frame_t;

/* CRC-8/MAXIM (poly 0x31, init 0x00) — CRC8("123456789") = 0xA1. */
static inline uint8_t frame_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* Command name for logging (e.g. "STATE"); unknown returns "0x%02x". */
const char *frame_cmd_name(uint8_t cmd);

/* Send one frame (adds SOF/CRC8/EOF). data NULL when len=0. Implemented by the TCP transport. */
esp_err_t frame_send(uint8_t frame_id, uint8_t cmd, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* FRAME_H */
