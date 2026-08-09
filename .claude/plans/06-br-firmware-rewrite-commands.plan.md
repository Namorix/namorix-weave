---
name: "BR Firmware Rewrite — Command Handlers"
overview: "Viết lại communicate_command.c: bảng handler, set_* dùng chung field descriptor, lock OT có helper, dedup trùng lặp."
todos: []
isProject: false
---

# BR Firmware Rewrite — Command Handlers

## Quy tắc đặt tên (thống nhất)

Theo **Rule 11** — `namorix-weave/.claude/CLAUDE.md`. Tóm tắt cho C: type `snake_case_t`, hàm `{module}_snake_case`, biến `snake_case`, static `s_` / module `m_`, hằng số `UPPER_SNAKE_CASE`, file `snake_case.c/.h`, include guard `UPPER_SNAKE_H`.

## Tình trạng hiện tại (từ review)

- 5 handler `set_*` (PANID/channel/name/xpanid/key) **copy-paste ~90%**: validate len → instance → lock → `otDatasetGetActive` → set 1 field + component bit → `otDatasetSetActive` → unlock → ACK/NACK.
- 3 handler table (router/child/joiner) gần giống nhau; **duplicate serializer** với `ot_table_snapshot.c` (một serializer, hai consumer).
- ~17 lần lặp `esp_openthread_get_instance() + lock_acquire + send_nack(0x03)`.
- **Factory reset trùng:** `br_main.c:on_boot_long_press` == `communicate_command.c:do_nvs_erase_and_restart`.
- **`thread_graceful_shutdown` trùng body** với `handle_thread_stop`.
- 2 mảng task/stack tách rời (`br_main.c:k_tasks` vs `communicate_command.c:s_br_health_tasks`) + define trong `br_config.h` → **3 nguồn sự thật**.
- **Bug:** fallback `otLinkGetExtendedAddress` trong `handle_mac_address` là **dead code sau `return`**.
- **Bất nhất spec/code:** doc nói CMD_FACTORY mang confirm byte `0xAA`; backend gửi, nhưng handler **bỏ qua** `data`.
- **IP_ADDR:** `s_pending_ip_frame_id` ghi từ 3 context không lock; chỉ giữ 1 request (request thứ 2 ghi đè); retry timer **vô hạn** nếu backend không ACK.

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
- **Task/stack:** 1 mảng duy nhất trong `br_config.h` (name + stack + prio) cấp cho cả task create lẫn CMD_BR_HEALTH TLV.

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
