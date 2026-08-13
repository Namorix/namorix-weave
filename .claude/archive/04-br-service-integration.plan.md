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
- Kết quả → cập nhật role, phát SignalR `border-router:state` nếu role đổi.

## Lifecycle (HostedService `BrConnectionService`)

```
StartAsync → connect BrTcpClient (retry backoff) → khởi động polling timer
StopAsync  → stop timer → disconnect → dispose
```

- Tái connect: BR chỉ accept **1 client** → luôn đóng socket cũ trước khi connect lại.
- Khi mất kết nối → phát `border-router:connection` (connected/false), bật backoff, không crash service.

## CMD_NOTIFY (0x45) — push từ BR

- DATA = 4 byte BE `changed_mask` (bit field) — đã chốt theo `ot_change_detector.h`:

| Bit | Value | Source | Hành động backend |
|-----|-------|--------|-------------------|
| 0 | `0x01` | ROLE | pull CMD_STATE → `border-router:state` |
| 1 | `0x02` | IP (leader RLOC) | pull CMD_IP_ADDR (refresh cache) |
| 2 | `0x04` | DATASET | pull CMD_DATASET_ACTIVE → `border-router:dataset` |
| 3 | `0x08` | ROUTER table | pull CMD_ROUTER_TABLE → `border-router:router-table` |
| 4 | `0x10` | CHILD table | pull CMD_CHILD_TABLE → `border-router:child-table` |
| 5 | `0x20` | JOINER table | pull CMD_JOINER_TABLE → `border-router:joiner-table` |

- Backend định nghĩa hằng số bit khớp (C#: `Role = 1<<0`, `Ip = 1<<1`, `Dataset = 1<<2`, `RouterTable = 1<<3`, `ChildTable = 1<<4`, `JoinerTable = 1<<5`) — không hardcode.
- Nhận NOTIFY → decode mask → query lại các nguồn tương ứng → phát SignalR event.

## SignalR events → frontend

| Event | Payload |
|---|---|
| `border-router:connection` | `{ connected, host }` |
| `border-router:state` | `{ role }` |
| `border-router:health` | `{ freeHeap, uptimeMs, tasks[] }` |
| `border-router:dataset` | `BrActiveDataset` |
| `border-router:router-table` | `RouterEntry[]` |
| `border-router:child-table` | `ChildEntry[]` |
| `border-router:joiner-table` | `JoinerEntry[]` |

- Gắn vào `WeaveService` (gRPC addon channel) hoặc `IHubContext` riêng; frontend dùng SignalR client có sẵn.

## Checklist

- [x] Options `BorderRouterOptions` + validate
- [x] BrConnectionService (Start/Stop/retry/poll)
- [x] Xử lý NOTIFY + changed_mask → query lại
- [x] SignalR hub + event push
- [x] Di trú state watchdog (không cho BR restart)

## Đã triển khai (2026-08-10)

- `Services/BorderRouterOptions.cs` — `BorderRouter` section (Host/Port/StatePollIntervalSec/RequestTimeoutMs), DataAnnotations + ValidateOnStart
- `Services/BrConnectionService.cs` — BackgroundService: Connected/Disconnected/FrameReceived events, STATE poll 5s (chống watchdog 75s), HEALTH push mỗi 3 tick (~15s), NOTIFY (0x45) → decode `BrChangedMask` → re-query các nguồn → SignalR
- `Hubs/BrHub.cs` — hub `/hubs/weave` (không có method, chỉ push server→client)
- `Models/BrChangedMask.cs` + `Parsers/BrNotifyParser.cs` — bit map khớp `ot_change_detector.h`, payload 4B BE
- `Dtos/BrDtos.cs` + `Dtos/BrDtoMapper.cs` — DTO camelCase khớp `frontend/src/types/network.ts` (rloc16 `0x{x4}`, extAddress colon-hex lower, role lowercase, dataset decode TLV)
- `BrTcpClient.cs` — thêm event `Connected`
- `Program.cs` — `AddSignalR()`, options binding + validate, singleton BrTcpClient/BrCommandClient (factory truyền timeout từ options), `AddHostedService<BrConnectionService>()`, `MapHub<BrHub>("/hubs/weave")`
- `appsettings.json` — section `BorderRouter`
- **Sửa bug Plan 03**: `MeshCopTlvType` constant sai so với OpenThread chuẩn (`NetworkKey` 0x06→0x05, `MeshLocalPrefix` 0x08→0x07, `ActiveTimestamp` 0x0b→0x0e) + thêm `Pskc` 0x04, `SecurityPolicy` 0x0c, `ChannelMask` 0x35
- **Chưa build** (user tự build)
