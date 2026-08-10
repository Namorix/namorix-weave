---
name: "BR Firmware Rewrite — Transport & Dispatch Core"
overview: "Gộp communicate.c + transport_tcp.c + communicate_queue.c thành một lớp frame/TCP gọn; dispatch bằng handler table."
todos: []
isProject: false
---

## ✅ Trạng thái: HOÀN THÀNH (2026-08-10)

Đã triển khai transport + dispatch firmware trong `namorix-weave/firmware/border-router-host/`. Khác biệt so với thiết kế gốc:
- API đặt tên theo prefix `frame_`/`command_` (Rule 11): `transport_init` → `frame_tcp_init`, `transport_send` → `frame_send(frame_id, cmd, data, len)`, RX loop trong `tcp_rx_task` + parser block-copy `rx_parse_and_dispatch`/`rx_append`.
- Wire constants + `frame_t` + CRC8 + enum `frame_nack_t` + `frame_cmd_name` gom vào `include/frame/frame.h` (header dùng chung cả firmware, không riêng transport).
- Handler table: signature thống nhất `int fn(frame_t *f)`; `command_dispatch()` trong `main/command/command.c`. STATE là handler thật duy nhất (ACK + đánh dấu state watchdog); 20 command còn lại NACK 0x02 not-ready (stub chờ Plan 06). CMD lạ → NACK invalid-cmd.
- Đồng bộ socket: 2 mutex `s_fd_mutex` (single-owner socket) + `s_tx_mutex` (single tx buffer); `frame_send` retry EAGAIN có giới hạn + xử lý partial send.
- Bỏ queue trung gian: RX parse → dispatch **đồng bộ** trong RX task → không còn clamp 256B âm thầm; frame ≤ 2048 luôn xử lý trọn vẹn.
- State watchdog (5×15s → `esp_restart`) giữ trong Plan 05 (transport keepalive); IP_ADDR retry để Plan 06.
- `main/main.c` placeholder (nvs + esp_netif + frame_tcp_init); boot đầy đủ (W5500/RCP/OT/LED/SRP) → Plan 07.
- `main/CMakeLists.txt` rút gọn: 3 source + `REQUIRES esp_netif nvs_flash lwip`.
- Chưa build (người dùng tự build); chưa commit.

# BR Firmware Rewrite — Transport & Dispatch Core

## Mục tiêu

Viết lại firmware br-host từ đầu (`namorix-thread/firmware/br-host`) để gọn và tối ưu. Plan này chuyển phần **transport + dispatch** (dựa trên review code hiện tại).

## Quy tắc đặt tên (thống nhất)

Theo **Rule 11** — `namorix-weave/.claude/CLAUDE.md`. Tóm tắt cho C: type `snake_case_t`, hàm `{module}_snake_case`, biến `snake_case`, static `s_` / module `m_`, hằng số `UPPER_SNAKE_CASE`, file `snake_case.c/.h`, include guard `UPPER_SNAKE_H`.

## Tình trạng hiện tại (từ review)

- `communicate.c` + `transport_tcp.c` + `communicate_queue.c` chia quá nhỏ, logic đan xen.
- `communicate_send_frame` **malloc** full buffer chỉ để CRC → có thể bỏ.
- Parser RX **memmove từng byte** (O(n²)).
- `process_task` (communicate_queue.c) là **if/else 20 nhánh** → dùng handler table.
- `s_client_fd` dùng chung giữa accept/rx/send task, **không lock** → race close/send (EBADF nuốt).
- `communicate_queue_post` clamp payload ở `ITEM_MAX_DATA=256` **âm thầm** (footgun).

## Thiết kế mục tiêu

### 1. Một module `transport/` duy nhất
Gộp thành `transport/frame_tcp.c`:
- `transport_init()` — bind TCP `:5000` (`CONFIG_BR_FRAME_TCP_PORT`), accept 1 client (đóng connection cũ khi connect mới).
- `transport_rx_loop()` — task đọc socket, tích luỹ vào ring buffer, parse frame.
- `transport_send(frame)` — build `[SOF][ID][CMD][LEN_H][LEN_L][DATA][CRC8][EOF]` vào **một buffer**, CRC tính trực tiếp trong lúc fill (không malloc), **một lần `write()`**.
- Gửi partial/fail → đếm lỗi + log; không `(void)`-cast bỏ qua.

### 2. Parser RX
- Giữ `SOF/EOF` scanning (không escape), nhưng đổi sang **chép block** thay vì memmove từng byte; hoặc index-con trỏ đơn giản với `LEN` biết trước.
- Validate: `LEN ≤ 2048`, EOF đúng `0x55`, CRC8 đúng.
- Frame hợp lệ → callback handler (đi thẳng, bỏ queue trung gian hoặc giữ queue chỉ để chống nghẽn).

### 3. Dispatch — handler table
```c
typedef struct { uint8_t cmd; int (*fn)(frame_t *f); } cmd_handler_t;
static const cmd_handler_t k_handlers[] = {
    { CMD_STATE, handle_state }, { CMD_IP_ADDR, handle_ip_addr }, /* ... */
};
```
- Không if/else dài; tra bảng bằng switch nhỏ hoặc binary search.
- CMD_NOTIFY (0x45) là BR→client push, không nằm trong bảng client-side.

### 4. Đồng bộ socket
- `s_client_fd` chỉ do **một task** (rx loop) quản lý hoặc bọc mutex nhỏ; `send` từ handler task dùng fd snapshot + kiểm tra hợp lệ.

### 5. NACK codes
- Định nghĩa enum: `0x01 INVALID_CMD, 0x02 NOT_READY, 0x03 TIMEOUT, 0x04 INVALID_PARAM, 0x05 BUSY` trong header dùng chung (hiện là magic number).

## GIỮ NGUYÊN (protocol-critical)

- Wire format: `[0xAA][FrameID][CMD][LEN_H][LEN_L][DATA][CRC8][0x55]`, không escape, min 7 byte.
- CRC-8/MAXIM: poly `0x31`, init `0x00`, input = `[FrameID, CMD, LEN_H, LEN_L, ...DATA]`.
- FrameID echo trên ACK/NACK; FrameID client sinh 0–255 wrap.
- **Chỉ 1 TCP client** (BR là server backlog 1).
- State watchdog: `esp_restart()` nếu không có CMD_STATE trong 5×15s.

## Checklist

- [x] Gộp 3 module → `transport/frame_tcp.c` (init / rx loop / send)
- [x] CRC inline + single buffer, bỏ malloc + memmove từng byte
- [x] Handler table thay if/else 20 nhánh
- [x] Mutex/single-owner cho `s_client_fd`; xử lý send partial/fail
- [x] Enum NACK code + header dùng chung
- [x] Bỏ clamp payload âm thầm (hoặc fail rõ ràng nếu quá 2048)
