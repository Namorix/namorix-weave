---
name: "BR Frame Protocol Core (C#)"
overview: "Lớp transport + codec frame protocol cho client TCP kết nối br-host (ESP32-S3)."
todos: []
isProject: false
---

# BR Frame Protocol Core — C#

## ✅ Trạng thái: HOÀN THÀNH (2026-08-10)

Đã triển khai toàn bộ `backend/src/BorderRouter/`. Khác biệt so với thiết kế gốc:
- Type frame đổi `Frame` → `BrFrame` (tránh xung đột namespace `…BorderRouter.Frame`).
- `TryParse` nhận `ReadOnlySpan<byte>` (1 frame hoàn chỉnh); thêm `TryParseNext(List<byte>, out BrFrame?)` xử lý tích luỹ/resync SOF/partial frame (dùng `CollectionsMarshal.AsSpan`, zero-copy).
- **Bỏ unit test** theo quyết định của user (chưa có test project trong cả hệ sinh thái).

## Quy tắc đặt tên (thống nhất)

Theo **Rule 11** — `namorix-weave/.claude/CLAUDE.md`. Tóm tắt cho C#: type `PascalCase`, method `PascalCase`, biến local/tham số `camelCase`, field private `_camelCase`, hằng số/enum value `PascalCase`, file `PascalCase.cs`, namespace dotted `PascalCase`.

## Nguồn spec

- `namorix-thread/documents/protocol/usb_cdc_frame_structure.md` (frame, CRC8, CMD table)
- Code gốc firmware: `namorix-thread/firmware/br-host/main/communicate/communicate.c`, `transport_tcp.c`

## Kiến trúc phía BR

- br-host là **TCP server** `0.0.0.0:5000` (`CONFIG_BR_FRAME_TCP_PORT`), accept **1 client** duy nhất (backlog 1). Client C# là TCP client duy nhất.
- Socket BR set `O_NONBLOCK`, đọc 256-byte chunks. Không mã hoá/không auth → chỉ chạy trusted LAN.

## Frame format (byte-level)

```
[SOF=0xAA][FrameID 1B][CMD 1B][LEN_H 1B][LEN_L 1B][DATA×LEN][CRC8 1B][EOF=0x55]
```

- LEN big-endian, **max 2048**. Frame tối thiểu 7 byte.
- Không escape: SOF/EOF chỉ có nghĩa ở đầu/cuối.

## CRC-8/MAXIM (poly 0x31, init 0x00)

Input = `[FrameID, CMD, LEN_H, LEN_L, ...DATA]` (KHÔNG gồm SOF/EOF). Tương đương C:

```csharp
static byte Crc8Maxim(ReadOnlySpan<byte> data)
{
    byte crc = 0;
    foreach (byte b in data)
    {
        crc ^= b;
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x80) != 0 ? (byte)((crc << 1) ^ 0x31) : (byte)(crc << 1);
    }
    return crc;
}
```

## Thành phần cần xây (backend/src/)

### 1. `Frame/Frame.cs`
- `record Frame(byte FrameId, byte Cmd, byte[] Data)`.

### 2. `Frame/FrameCodec.cs`
- `byte[] Encode(Frame f)` — build header, tính CRC8, thêm SOF/EOF.
- `Frame? TryParse(ref ArrayBuffer<byte> buf)` — tích luỹ byte, tìm SOF, đọc LEN, validate LEN ≤ 2048, check EOF + CRC. Phải xử lý **partial frame** (dữ liệu về từng mảnh).

### 3. `Frame/Crc8Maxim.cs`
- Hàm CRC8 ở trên (unit test với vector biết trước).

### 4. `BorderRouter/BrTcpClient.cs`
- Wrap `TcpClient` + `NetworkStream`: `ConnectAsync(host, port)`, `SendAsync(byte[])`, dòng đọc background → đẩy vào parser → callback frame hợp lệ.
- Reconnect với backoff; lưu ý: **chỉ 1 client**, đóng socket cũ trước khi connect lại.

## Pending map (request/response)

- `FrameId` tăng dần `0–255`, wrap.
- `Dictionary<byte, TaskCompletionSource<Frame>>` — ghép ACK/NACK theo FrameId, timeout mỗi request (vd. 3–5s).
- BR echo cùng FrameId trong ACK/NACK (`usb_cdc_frame_structure.md` §8).

## Checklist

- [x] FrameCodec encode/parse (Encode + TryParse + TryParseNext) — unit test bỏ theo user
- [x] Crc8Maxim (CRC-8/MAXIM, poly 0x31, init 0x00)
- [x] BrTcpClient connect/send/receive + reconnect (backoff 1s→30s)
- [x] Pending map + timeout + FrameId rotation (PendingFrameStore)
