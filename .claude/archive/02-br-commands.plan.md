---
name: "BR Commands Layer (C#)"
overview: "Lớp command request/response giữa backend C# và br-host, kèm handshake đặc biệt."
todos: []
isProject: false
---

# BR Commands — C#

## ✅ Trạng thái: HOÀN THÀNH (2026-08-10)

Đã triển khai `backend/src/BorderRouter/`: `Commands.cs` (enum `BrCommand`), `BrNackException.cs` (enum `BrNackCode` + exception), `BrCommandClient.cs`. Khác biệt so với thiết kế gốc:
- CMD constants dùng `enum BrCommand : byte` thay cho const riêng lẻ (typed hơn).
- `BrNackCode` đặt chung file `BrNackException.cs` theo plan.
- Handshake IP_ADDR: sau khi nhận ACK 16B, gửi ACK rỗng qua `transport.SendFrameAsync` (fire-and-forget, không register pending vì BR không trả lời tiếp).
- `FactoryResetAsync` giữ confirm byte `0xAA` theo spec hiện tại (mở — plan firmware 06 muốn bỏ nhưng chưa chốt).
- Timeout request mặc định 3s (`requestTimeout` có thể cấu hình qua constructor).

## Quy tắc đặt tên (thống nhất)

Theo **Rule 11** — `namorix-weave/.claude/CLAUDE.md`. Tóm tắt cho C#: type `PascalCase`, method `PascalCase`, biến local/tham số `camelCase`, field private `_camelCase`, hằng số/enum value `PascalCase`, file `PascalCase.cs`, namespace dotted `PascalCase`.

## Nguồn spec

- `namorix-thread/documents/protocol/usb_cdc_frame_structure.md` (CMD table, DATA payload, NACK codes)
- Code firmware: `namorix-thread/firmware/br-host/main/communicate/communicate_command.c`, `communicate_task.c`

## Command constants

Copy bảng CMD từ `communicate.h` sang `Frame/Commands.cs`:

```
DATA=0x01 ACK=0x02 NACK=0x03
RESET=0x10 FACTORY=0x11 STATE=0x12 IP_ADDR=0x13 DATASET_ACTIVE=0x14 MAC_ADDRESS=0x16 BR_HEALTH=0x17
SET_PANID=0x20 SET_CHANNEL=0x21 SET_NETWORK_NAME=0x22 SET_EXTENDED_PANID=0x23 SET_NETWORK_KEY=0x24
ROUTER_TABLE=0x30 CHILD_TABLE=0x31 JOINER_TABLE=0x32
THREAD_START=0x40 THREAD_STOP=0x41 THREAD_VERSION=0x42 COMMISSIONER_JOINER=0x43 SRP_REGISTER=0x44
NOTIFY=0x45 (BR → client, push)
```

## Luồng pull (client → BR)

| CMD | DATA gửi | ACK trả về |
|---|---|---|
| STATE | 0 | 1 byte role (0 disabled → 4 leader) |
| IP_ADDR | 0 | **16 bytes IPv6 Leader RLOC** |
| DATASET_ACTIVE | 0 | raw Active Dataset TLVs |
| MAC_ADDRESS | 0 | 8 bytes EUI-64 |
| BR_HEALTH | 0 | 16-byte prefix + TLV suffix |
| ROUTER/CHILD/JOINER_TABLE | 0 | count(1) + entries |
| THREAD_VERSION | 0 | UTF-8 ≤64 |
| SET_* | theo field | 0 |
| THREAD_START/STOP | 0 | 0 |
| COMMISSIONER_JOINER | EUI64(8)+PSKD_len(1)+PSKD(6–32)+Timeout(4 BE) | 0 |
| SRP_REGISTER | hostname_len(1)+hostname+IPv6(16)+port(2 BE) | 0 |
| RESET / FACTORY | 0 | ACK rồi BR restart sau 2s |

## ⚠️ Handshake đặc biệt: CMD_IP_ADDR

3 bước — bắt buộc, nếu không BR retry ACK mỗi 1s vô hạn:

```
Client→BR:  CMD_IP_ADDR (frameId=N)
BR→Client:  CMD_ACK (N, 16-byte RLOC)  [BR đặt s_pending=N + start timer 1s]
Client→BR:  CMD_ACK rỗng (N)            → BR dừng retry
```

## NACK codes (DATA = 1 byte)

`0x01` invalid cmd · `0x02` not ready · `0x03` timeout · `0x04` invalid param · `0x05` busy

## Thành phần cần xây (backend/src/)

### `BorderRouter/BrCommandClient.cs`
- `Task<byte[]> RequestAsync(byte cmd, byte[] data, CancellationToken ct)` — gửi frame, chờ ACK/NACK theo FrameId, throw `BrNackException(code)` nếu NACK.
- Method tiện: `GetStateAsync`, `GetIpAddrAsync` (tự động trả ACK rỗng), `GetDatasetActiveAsync`, `GetMacAsync`, `GetBrHealthAsync`, `GetRouterTableAsync`, `GetChildTableAsync`, `GetJoinerTableAsync`, `SetPanIdAsync`, `SetChannelAsync`, `SetNetworkNameAsync`, `SetExtendedPanIdAsync`, `SetNetworkKeyAsync`, `StartThreadAsync`, `StopThreadAsync`, `GetThreadVersionAsync`, `AddJoinerAsync`, `RegisterSrpAsync`, `ResetAsync`, `FactoryResetAsync`.

### `BorderRouter/BrNackException.cs`
- Chứa `NackCode`.

## Checklist

- [x] Commands.cs constants (`enum BrCommand`)
- [x] BrCommandClient.RequestAsync (NACK → `BrNackException`)
- [x] Xử lý riêng IP_ADDR (tự trả ACK rỗng)
- [x] Wrapper method cho từng CMD (19 method)
