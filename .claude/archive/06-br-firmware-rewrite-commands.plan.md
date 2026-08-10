---
name: "BR Firmware Rewrite — Command Handlers"
overview: "Viết lại communicate_command.c: bảng handler, set_* dùng chung field descriptor, lock OT có helper, dedup trùng lặp."
todos: []
isProject: false
---

# BR Firmware Rewrite — Command Handlers

## Quy tắc đặt tên (thống nhất)

Theo **Rule 11** — `namorix-weave/.claude/CLAUDE.md`. Tóm tắt cho C: type `snake_case_t`, hàm `{module}_snake_case`, biến `snake_case`, static `s_` / module `m_`, hằng số `UPPER_SNAKE_CASE`, file `snake_case.c/.h`, include guard `UPPER_SNAKE_H`.

## Tình trạng hiện tại (sau Plan 07 — cấu trúc file mới)

> Plan 07 đã port firmware sang `firmware/border-router-host` theo Rule 11. `command.c` hiện là **stub toàn bộ**: mọi CMD (trừ `STATE` = keepalive ACK) trả `NACK_NOT_READY`. Plan 06 = viết handler thật.

- **File mới:** `main/command/command.c` (bảng `s_handlers[]` + `command_dispatch`), `main/transport/frame_tcp.c` (state watchdog + `frame_send`), `main/openthread/ot_table_snapshot.c` (serializer tables), `main/openthread/ot_change_detector.c` (push CMD_NOTIFY), `include/br_config.h` (task name/stack/prio tập trung — ✅ đã xong ở Plan 07).
- 5 handler `set_*` (PANID/channel/name/xpanid/key) cần viết — dùng chung 1 dataset mutator (field descriptor) để tránh copy-paste ~90%.
- 3 handler table (router/child/joiner) → generic reader dùng chung serializer `ot_table_snapshot.c`.
- ~18 CMD trong `s_handlers[]` cần `esp_openthread_get_instance() + lock_acquire` — cần scoped lock helper trả false → NACK 0x03 (TIMEOUT).
- **Factory reset:** boot button (`main/main.c:on_boot_long_press`) viết trực tiếp; CMD_FACTORY còn stub → gom về 1 `do_factory_reset()` dùng chung.
- **Thread stop/start:** CMD_THREAD_START/STOP còn stub → tránh trùng body giữa graceful shutdown và stop.
- **BR_HEALTH TLV tasks:** k_tasks đang là static trong `main/main.c` → chuyển 1 bảng duy nhất trong `br_config.h` (name+stack+prio) cấp cho cả task create lẫn TLV 0x01/0x02/0x03.
- **Bug cũ (từ reference, chưa có ở stub mới):** fallback `otLinkGetExtendedAddress` sau `return` trong MAC là dead code — viết mới đừng tái phạm.
- **Bất nhất spec/code:** doc nói CMD_FACTORY mang confirm byte `0xAA`; backend gửi, nhưng handler bỏ qua `data` → chốt lại (ưu tiên **bỏ confirm byte**: frame đã có CRC + FrameID).
- **IP_ADDR handshake 3 bước:** request → ACK + 16B RLOC + timer 1s → client ACK rỗng để dừng retry; cap retry thay vì vô hạn.

## Thiết kế mục tiêu

### 1. Scoped OT lock helper
```c
// MỌI OT API từ task ngoài phải trong lock. Trả lỗi rồi NACK 0x03 (TIMEOUT) nếu không lấy được.
#define OT_LOCK_TIMEOUT_MS 1000
bool ot_lock(void);          // acquire + trả false nếu fail
void ot_unlock(void);        // release
```
- Handlers gọi cặp này, không tự gọi API trực tiếp. Riêng Commissioner giữ ngoại lệ cũ (release lock khi poll state).

### 2. Gộp 5 `set_*` → một dataset mutator
```c
typedef struct {
    uint8_t cmd; uint8_t tlv_type; uint8_t min_len; uint8_t max_len;
    void (*apply)(otOperationalDataset *d, const uint8_t *data, size_t len);
    uint32_t component;
} dataset_field_t;
```
- Một hàm `handle_dataset_set(field, frame)` → validate → lock → get active → apply → set active → unlock → ACK.
- Bảng field: PANID (2 BE), CHANNEL (1, 11–26), NETWORK_NAME (utf8), EXTENDED_PANID (8), NETWORK_KEY (16).

### 3. Gộp 3 table handler → generic reader
- Một `handle_table_read(cmd, serializer)` dùng chung serializer từ `ot_table_snapshot.c`.
- Xóa hẳn serializer riêng trong handler (reuse snapshot builder).

### 4. Dedup
- **Factory reset:** 1 hàm `do_factory_reset()` dùng cho cả boot button (long press) và CMD_FACTORY.
- **Graceful shutdown:** 1 hàm dùng chung cho CMD_THREAD_STOP và shutdown trước reset.
- **Task/stack:** `br_config.h` đã có define name/stack/prio (Plan 07). Còn lại: đưa mảng `k_tasks` (name+stack+prio) vào `br_config.h` — dùng chung cho task create (`main/main.c`) lẫn CMD_BR_HEALTH TLV.

### 5. Fix cụ thể
- `handle_mac_address`: bỏ dead code; dọn lại chuỗi fallback (factory EUI64 → extended → `esp_read_mac`).
- **CMD_FACTORY confirm byte:** chốt lại trong spec — hoặc spec bỏ `0xAA`, hoặc handler validate. Ưu tiên: **spec bỏ confirm byte** (kênh frame đã có CRC + FrameID; byte thừa không có giá trị) → cập nhật `usb_cdc_frame_structure.md` + backend.
- **IP_ADDR:** bọc `s_pending_ip_frame_id` bằng mutex hoặc cho rx task là single-owner; **cap retry** (vd. N lần rồi dừng + log) thay vì retry vô hạn.

## GIỮ NGUYÊN

- Ngữ nghĩa từng CMD + format payload (STATE role 0–4, dataset TLVs, MAC 8B EUI-64, BR_HEALTH 16B prefix + TLV 0x01/0x02/0x03, tables count-first-byte).
- **Handshake IP_ADDR 3 bước:** request → ACK + 16B RLOC + timer 1s → client ACK rỗng cùng frame_id để dừng retry.
- **SRP pointer-lifetime:** buffer tĩnh `s_srp_hostname` / `s_srp_backend_addr` (OT chỉ lưu con trỏ, không copy); không gọi `otSrpClientStart(NULL)` (crash deref).
- Watchdog 5×15s; ngoại lệ Commissioner lock.
- **Không giữ OT lock qua NVS erase/write-back** (factory reset raw erase, không stop OT trước).

## Checklist

- [ ] Scoped OT lock helper + áp dụng mọi handler
- [ ] Dataset field descriptor → 1 hàm set_*
- [ ] Generic table reader reuse serializer
- [ ] Dedup factory reset / graceful shutdown / task table
- [ ] Fix dead code MAC, IP_ADDR mutex + retry cap
- [ ] Chốt CMD_FACTORY confirm byte (spec + backend + firmware đồng bộ)
