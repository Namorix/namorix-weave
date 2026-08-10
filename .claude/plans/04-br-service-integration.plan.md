---
name: "BR Service Integration (C#)"
overview: "Hosted service ASP.NET Core: lifecycle, STATE polling, xử lý NOTIFY, đẩy SignalR lên frontend."
todos: []
isProject: false
---

# BR Service Integration — C#

## Quy tắc đặt tên (thống nhất)

Theo **Rule 11** — `namorix-weave/.claude/CLAUDE.md`. Tóm tắt cho C#: type `PascalCase`, method `PascalCase`, biến local/tham số `camelCase`, field private `_camelCase`, hằng số/enum value `PascalCase`, file `PascalCase.cs`, namespace dotted `PascalCase`.

## Nguồn spec

- `firmware/border-router-host/main/transport/frame_tcp.c` (state watchdog 5×15s → restart)
- `firmware/border-router-host/main/openthread/ot_change_detector.c` (push CMD_NOTIFY → changed_mask)
- `firmware/border-router-host/main/command/command.c` (bảng CMD, payload từng handler — theo Plan 06)

## Cấu hình (appsettings.json)

```json
{
  "BorderRouter": {
    "Host": "192.168.1.10",
    "Port": 5000,
    "StatePollIntervalSec": 5,
    "RequestTimeoutMs": 4000
  }
}
```

## STATE polling — bắt buộc

- BR **restart** nếu không nhận `CMD_STATE` trong 5×15s = 75s (`communicate_task.c`).
- Poll `CMD_STATE` mỗi **5s** (giống Node backend) — an toàn so với ngưỡng 75s.
- Kết quả → cập nhật role, phát SignalR `weave:br-state` nếu role đổi.

## Lifecycle (HostedService `BrConnectionService`)

```
StartAsync → connect BrTcpClient (retry backoff) → khởi động polling timer
StopAsync  → stop timer → disconnect → dispose
```

- Tái connect: BR chỉ accept **1 client** → luôn đóng socket cũ trước khi connect lại.
- Khi mất kết nối → phát `weave:br-connection` (connected/false), bật backoff, không crash service.

## CMD_NOTIFY (0x45) — push từ BR

- DATA = 4 byte BE `changed_mask` (bit field: bit0 dataset, bit1 router table, bit2 child table, ...).
- Nhận NOTIFY → query lại bảng tương ứng theo bit → phát SignalR event.
- (Cần xác nhận map bit → source: `ot_change_detector.c`.)

## SignalR events → frontend

| Event | Payload |
|---|---|
| `weave:br-connection` | `{ connected, host }` |
| `weave:br-state` | `{ role }` |
| `weave:br-health` | `{ freeHeap, uptimeMs, tasks[] }` |
| `weave:router-table` | `RouterEntry[]` |
| `weave:child-table` | `ChildEntry[]` |
| `weave:joiner-table` | `JoinerEntry[]` |

- Gắn vào `WeaveService` (gRPC addon channel) hoặc `IHubContext` riêng; frontend dùng SignalR client có sẵn.

## Checklist

- [ ] Options `BorderRouterOptions` + validate
- [ ] BrConnectionService (Start/Stop/retry/poll)
- [ ] Xử lý NOTIFY + changed_mask → query lại
- [ ] SignalR hub + event push
- [ ] Di trú state watchdog (không cho BR restart)
