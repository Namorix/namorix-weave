---
name: "BR Payload Parsers (C#)"
overview: "Parse DATA payload từ BR thành DTO C#: STATE, dataset, MAC, BR_HEALTH, các bảng routing."
todos: []
isProject: false
---

# BR Payload Parsers — C#

## ✅ Trạng thái: HOÀN THÀNH (2026-08-10)

Đã triển khai `backend/src/BorderRouter/Models/` (DTO) + `backend/src/BorderRouter/Parsers/` (parser). Khác biệt so với thiết kế gốc:
- DTO dùng prefix `Br` cho nhất quán codebase (plan ghi `MacAddress` → `BrMacAddress`).
- Exceptions gom vào folder `BorderRouter/Exceptions/`: `BrNackException.cs` (từ Plan 02) + `BrPayloadException.cs` mới, namespace `Namorix.Weave.BorderRouter.Exceptions`.
- `BrActiveDataset` giữ `byte[] Raw` + `FindTlv(byte type)` + class `MeshCopTlvType` (chưa parse chi tiết — đúng plan).
- Parser nhận `ReadOnlySpan<byte>` → throw `BrPayloadException` khi thiếu byte (protect chống buffer dịch vụ).
- Joiner entry đã xác nhận lại từ spec: **variable-length** (`[Type][SharedId][PSKD_len][PSKD][ExpirationTime(4 BE)]`), không phải 14B cố định như plan cũ — parser bước theo Type/SharedId/PSKD_len/PSKD/ExpirationTime.
- Không unit test (theo yêu cầu người dùng).

## Quy tắc đặt tên (thống nhất)

Theo **Rule 11** — `namorix-weave/.claude/CLAUDE.md`. Tóm tắt cho C#: type `PascalCase`, method `PascalCase`, biến local/tham số `camelCase`, field private `_camelCase`, hằng số/enum value `PascalCase`, file `PascalCase.cs`, namespace dotted `PascalCase`.

## Nguồn spec

- `namorix-thread/documents/protocol/usb_cdc_frame_structure.md` §5 (DATA payload từng CMD)
- `namorix-thread/documents/protocol/table_data_format.md` (Router/Child/Joiner binary layout)

## Các DTO (Backend/BorderRouter/Models/)

### STATE — 1 byte role
`0` disabled · `1` detached · `2` child · `3` router · `4` leader

```csharp
public enum BrRole : byte { Disabled = 0, Detached = 1, Child = 2, Router = 3, Leader = 4 }
```

### DATASET_ACTIVE — raw Active Dataset TLVs
- Không parse chi tiết ngay; giữ `byte[] Raw` + helper tìm TLV theo type (`MeshLocalPrefix`, `NetworkName`, `PanId`, `Channel`, ...).
- Type constants theo OpenThread MeshCop TLV.

### MAC_ADDRESS — 8 bytes EUI-64
```csharp
public readonly record struct MacAddress(byte[] Value /* 8 */); // ToString hex "AA:BB:..."
```

### BR_HEALTH — 16-byte prefix + TLV suffix
Prefix: 4 × uint32 BE → `FreeHeap`, `MinFreeHeap`, `UptimeMs`, `MleDetachCount`.
Suffix TLV: Type `0x01` task_name (utf8), `0x02` high_water_mark (u32 BE), `0x03` stack_size (u32 BE). Nhiều entry theo thứ tự xuất hiện.

### ROUTER_TABLE — count(1) + 15 bytes/entry
`RouterId(1) RLOC16(2 BE) ExtAddr(8) LQin(1) LQout(1) Age(2 BE)`

### CHILD_TABLE — count(1) + 17 bytes/entry
`ChildId(1) RLOC16(2 BE) ExtAddr(8) LQin(1) AvgRssi(1 signed) FTD(1) RxOnIdle(1) Age(2 BE)`

### JOINER_TABLE — count(1) + entries **variable-length**
> ✅ Đã xác nhận lại từ `table_data_format.md`: entry KHÔNG cố định 14B (`Eui64(8) PanId(2) Timestamp(4)` là giả định sai trong plan cũ). Layout thật như dưới.

Entry: `[Type(1)] + [SharedId] + [PSKD_length(1)] + [PSKD] + [ExpirationTime(4 BE)]`

- **Type (1 byte):**
  - `0x00` = `Any` — SharedId = 8 byte padding `0x00`
  - `0x01` = `Eui64` — SharedId = 8 byte EUI-64
  - `0x02` = `Discerner` — SharedId = 1 byte length (bits, 1–64) + `ceil(len/8)` byte value (big-endian)
- **PSKD_length (1 byte) + PSKD** — utf8 string, không null-terminated (0–32)
- **ExpirationTime (4 byte uint32 BE)** — milliseconds; `0` = không expire

Parser phải **bước từng entry theo header** (đọc Type → nhảy SharedId theo Type → đọc PSKD_len → nhảy PSKD → đọc ExpirationTime), không loop offset cố định như Router/Child.

```csharp
public enum BrJoinerType : byte { Any = 0, Eui64 = 1, Discerner = 2 }

public readonly record struct BrJoinerDiscerner(int BitLength, byte[] Value);

public sealed record BrJoinerEntry(
    BrJoinerType Type,
    byte[]? Eui64,               // Type = Eui64
    BrJoinerDiscerner? Discerner, // Type = Discerner
    string Pskd,
    uint ExpirationTimeMs);      // 0 = không expire
```

## Nguyên tắc chung

- Dùng `System.Buffers.Binary.BinaryPrimitives` (ReadUInt16BigEndian, ReadUInt32BigEndian).
- Parser nhận `ReadOnlySpan<byte>` + độ dài kỳ vọng → throw `BrPayloadException` nếu thiếu byte (protect chống buffer dịch vụ).
- Không dùng BitConverter (endianness phụ thuộc host).

## Checklist

- [x] Models + enum BrRole
- [x] BrPayloadException
- [x] STATE / MAC / BR_HEALTH parser
- [x] Router / Child / Joiner table parser
- [x] Xác nhận lại joiner entry layout từ spec (variable-length, không phải 14B cố định)
