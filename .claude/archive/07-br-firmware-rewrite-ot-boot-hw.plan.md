---
name: "BR Firmware Rewrite — OT Detector, Boot & Hardware"
overview: "Dọn ot_change_detector, thống nhất boot flow + LED/button, giữ nguyên hành vi OT & SRP. ✅ HOÀN TẤT."
todos: []
isProject: false
status: done
---

# ✅ BR Firmware Rewrite — OT Detector, Boot & Hardware

> **Trạng thái: HOÀN TẤT (2026-08-10).** Port xong từ `namorix-thread/firmware/br-host` sang `firmware/border-router-host` theo Rule 11. Kết nối RCP SPI (S3 ↔ H2) đã được xác nhận chạy ngon.

## Cấu trúc mới đã triển khai

- `include/br_config.h` — config tập trung: task name/stack/prio + macro `ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG()` (SPI RCP, dùng `CONFIG_BR_RCP_*_GPIO`), `HOST_CONFIG()` (NONE), `PORT_CONFIG()`.
- `main/openthread/ot_launch.c` + `include/openthread/ot_launch.h` — boot OT, leader-weight boost (+16), preferred partition id, console.
- `main/openthread/dataset_init.c` — dataset chỉ khi chưa có active (`ESP-BR-<MAC>`).
- `main/openthread/ot_change_detector.c` — **slim**: bỏ `s_pending_flags_u32` / `s_changed_mask` / `get_and_clear()` / wrapper thừa; dùng chung serializer.
- `main/openthread/ot_table_snapshot.c` — serializer tables dùng chung (detector + handler).
- `main/rcp/rcp_ctrl.c` — RESET/BOOT pins (S3 GPIO7/GPIO8 → H2 RESET/BOOT=GPIO9).
- `main/hardware/led_status.c` — WS2812, giữ **last-known role** khi lock timeout.
- `main/hardware/boot_btn.c` — long press 3s → factory reset chung.
- `main/console/console.c` + `include/console/console.h` — REPL esp_console (UART/USB-CDC/USB-SERIAL-JTAG), gọi `register_system()`.
- `components/cmd_system/` — copy nguyên vẹn từ reference (vendor component).
- `main/main.c` — boot flow gom đúng thứ tự tham chiếu (eventfd → NVS → netif → event loop → W5500 backbone → mDNS → RCP reset → OT start → change detector → dataset → BR + SRP → frame TCP → LED → button → stack monitor).
- `main/transport/frame_tcp.c` — bỏ define task local, dùng macro từ `include/br_config.h`.

## Ghi chú kết nối RCP (đã verify)

- S3 SPI2 (host=1) ↔ H2: SCLK GPIO4→GPIO0, MOSI GPIO5→GPIO3, MISO GPIO2→GPIO1, CS GPIO3→GPIO2, IRQ GPIO1←GPIO4.
- S3 GPIO7 → H2 RESET, S3 GPIO8 → H2 BOOT (strapping GPIO9).
- W5500 dùng SPI3 (GPIO10/11/12/13) — không trùng RCP.
- UART để dành flash/update RCP (mặc định `CONFIG_ESP_CONSOLE_UART_DEFAULT`).

---

# Bản gốc (nội dung lập plan ban đầu)

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

- [x] Xóa dead state / wrapper thừa trong ot_change_detector
- [x] Một serializer tables dùng chung (detector + handlers)
- [x] Gom boot flow; giữ nguyên thứ tự hành vi
- [x] LED last-known role; boot button → do_factory_reset chung
- [x] Task config tập trung 1 nơi
