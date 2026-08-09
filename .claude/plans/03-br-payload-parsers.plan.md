---
name: "BR Payload Parsers (C#)"
overview: "Parse DATA payload từ BR thành DTO C#: STATE, dataset, MAC, BR_HEALTH, các bảng routing."
todos: []
isProject: false
---

# BR Payload Parsers — C#

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

### JOINER_TABLE — count(1) + entries biến đổi
Entry: `Eui64(8) PanId(2 BE) Timestamp(4 BE)` → độ dài cố định 14/entry; cần đọc kỹ lại spec để xác nhận layout chính xác trước khi code.

## Nguyên tắc chung

- Dùng `System.Buffers.Binary.BinaryPrimitives` (ReadUInt16BigEndian, ReadUInt32BigEndian).
- Parser nhận `ReadOnlySpan<byte>` + độ dài kỳ vọng → throw `BrPayloadException` nếu thiếu byte (protect chống buffer dịch vụ).
- Không dùng BitConverter (endianness phụ thuộc host).

## Checklist

- [ ] Models + enum BrRole
- [ ] BrPayloadException
- [ ] STATE / MAC / BR_HEALTH parser + unit test
- [ ] Router / Child / Joiner table parser + unit test
- [ ] Xác nhận lại joiner entry layout từ spec
