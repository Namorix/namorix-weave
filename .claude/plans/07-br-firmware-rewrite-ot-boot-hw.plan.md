---
name: "BR Firmware Rewrite — OT Detector, Boot & Hardware"
overview: "Dọn ot_change_detector, thống nhất boot flow + LED/button, giữ nguyên hành vi OT & SRP."
todos: []
isProject: false
---

# BR Firmware Rewrite — OT Detector, Boot & Hardware

## Quy tắc đặt tên (thống nhất)

Theo **Rule 11** — `namorix-weave/.claude/CLAUDE.md`. Tóm tắt cho C: type `snake_case_t`, hàm `{module}_snake_case`, biến `snake_case`, static `s_` / module `m_`, hằng số `UPPER_SNAKE_CASE`, file `snake_case.c/.h`, include guard `UPPER_SNAKE_H`.

## Tình trạng hiện tại (từ review)

- `ot_change_detector.c`: 4 wrapper thừa (`take_and_clear_u32`/`or_u32`/variants); `s_pending_flags_u32` chỉ để log = **dead state**; `s_changed_mask` + `get_and_clear()` **không ai gọi** (CMD_NOTIFY push thẳng). Table serialization bị **duplicate** giữa detector và handler on-demand.
- `snapshots_t` ~1.5 KB trên stack 10 KB (OK nhưng lớn).
- Boot flow trải qua `br_main.c` + `br_launch.c` + `br_rcp_ctrl.c` — logic rời, dễ lệch.

## Thiết kế mục tiêu

### 1. `openthread/ot_change_detector.c` — gọn lại
- **Xóa:** `s_pending_flags_u32`, `s_changed_mask` + `get_and_clear()` (vô dụng), các wrapper thừa.
- Giữ: hook `otSetStateChangedCallback()` → debounce (arm-once) → build snapshot + diff → `changed_mask` (u32 BE) → push `CMD_NOTIFY (0x45)`.
- **Một serializer duy nhất** cho tables (router/child/joiner) trong `ot_table_snapshot.c`, dùng chung cho cả detector diff lẫn handler `CMD_ROUTER/CHILD/JOINER_TABLE` (liên kết với plan 02).
- Giảm snapshot nếu được: chỉ snapshot các field thực sự cần diff.

### 2. Boot flow — gom logic
- Một file `boot.c` (hoặc giữ `br_main.c`) với thứ tự rõ:
```
nvs → netif → event loop → backhaul W5500 (IPv4 DHCP, timeout → restart)
→ mDNS → RCP reset → OT start (leader-weight boost + preferred partition id)
→ change detector → dataset init (chỉ khi chưa có active) → border router + SRP server
→ transport init → LED/button → stack monitor
```
- `br_rcp_ctrl.c` giữ nguyên API (RESET GPIO7 / BOOT GPIO8) nhưng chỉ còn hàm cần dùng.
- Không đổi thứ tự khởi động mang tính hành vi.

### 3. Hardware
- `led_status.c`: giữ poll role + **last-known role** khi lock timeout (fix nháy đỏ sai lúc joiner join) — không tái phạm bug DISABLED mặc định.
- `boot_btn.c`: giữ long-press 3s → factory reset; gọi chung `do_factory_reset()` (plan 02).

### 4. Config
- Mọi task (name/stack/prio) nằm trong `br_config.h` duy nhất (liên kết plan 02). Chú ý `configMAX_TASK_NAME_LEN = 16` → tên ≤ 15 ký tự.

## GIỮ NGUYÊN

- Leader-weight boost + preferred partition id (hành vi boot).
- Dataset init **chỉ khi không có active dataset**.
- `otSrpServerSetEnabled` sau border router init; SRP server chỉ listen trên Thread mesh.
- Border routing + prefix (RA/RIO, RIO lifetime).
- Thứ tự: backhaul có backbone trước khi OT start; IPv4 timeout → restart.

## Checklist

- [ ] Xóa dead state / wrapper thừa trong ot_change_detector
- [ ] Một serializer tables dùng chung (detector + handlers)
- [ ] Gom boot flow; giữ nguyên thứ tự hành vi
- [ ] LED last-known role; boot button → do_factory_reset chung
- [ ] Task config tập trung 1 nơi
