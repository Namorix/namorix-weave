using System.Buffers.Binary;
using System.Text;
using Namorix.Weave.BorderRouter.Exceptions;
using Namorix.Weave.BorderRouter.Models;

namespace Namorix.Weave.BorderRouter.Parsers;

public static class BrTableParser
{
    private const int RouterEntryLength = 15;
    private const int ChildEntryLength = 17;
    private const int Eui64Length = 8;
    private const int JoinerAnyPaddingLength = 8;

    public static IReadOnlyList<BrRouterEntry> ParseRouterTable(ReadOnlySpan<byte> payload)
    {
        if (payload.Length < 1)
            throw new BrPayloadException("ROUTER_TABLE payload missing count byte.");

        var count = payload[0];
        var required = 1 + count * RouterEntryLength;
        if (payload.Length < required)
            throw new BrPayloadException($"ROUTER_TABLE payload needs {required} bytes for {count} entries, got {payload.Length}.");

        var entries = new List<BrRouterEntry>(count);
        var offset = 1;
        for (var i = 0; i < count; i++)
        {
            entries.Add(new BrRouterEntry(
                payload[offset],
                BinaryPrimitives.ReadUInt16BigEndian(payload[(offset + 1)..(offset + 3)]),
                payload.Slice(offset + 3, Eui64Length).ToArray(),
                payload[offset + 11],
                payload[offset + 12],
                BinaryPrimitives.ReadUInt16BigEndian(payload[(offset + 13)..(offset + 15)])));
            offset += RouterEntryLength;
        }
        return entries;
    }

    public static IReadOnlyList<BrChildEntry> ParseChildTable(ReadOnlySpan<byte> payload)
    {
        if (payload.Length < 1)
            throw new BrPayloadException("CHILD_TABLE payload missing count byte.");

        var count = payload[0];
        var required = 1 + count * ChildEntryLength;
        if (payload.Length < required)
            throw new BrPayloadException($"CHILD_TABLE payload needs {required} bytes for {count} entries, got {payload.Length}.");

        var entries = new List<BrChildEntry>(count);
        var offset = 1;
        for (var i = 0; i < count; i++)
        {
            entries.Add(new BrChildEntry(
                payload[offset],
                BinaryPrimitives.ReadUInt16BigEndian(payload[(offset + 1)..(offset + 3)]),
                [.. payload.Slice(offset + 3, Eui64Length)],
                payload[offset + 11],
                (sbyte)payload[offset + 12],
                payload[offset + 13] == 1,
                payload[offset + 14] == 1,
                BinaryPrimitives.ReadUInt16BigEndian(payload[(offset + 15)..(offset + 17)])));
            offset += ChildEntryLength;
        }
        return entries;
    }

    public static IReadOnlyList<BrJoinerEntry> ParseJoinerTable(ReadOnlySpan<byte> payload)
    {
        if (payload.Length < 1)
            throw new BrPayloadException("JOINER_TABLE payload missing count byte.");

        var count = payload[0];
        var entries = new List<BrJoinerEntry>(count);
        var offset = 1;

        for (var i = 0; i < count; i++)
        {
            if (offset >= payload.Length)
                throw new BrPayloadException($"JOINER_TABLE entry {i} missing Type byte.");

            var type = (BrJoinerType)payload[offset++];
            byte[]? eui64 = null;
            BrJoinerDiscerner? discerner = null;

            switch (type)
            {
                case BrJoinerType.Any:
                    if (payload.Length - offset < JoinerAnyPaddingLength)
                        throw new BrPayloadException($"JOINER_TABLE entry {i} missing ANY padding.");
                    offset += JoinerAnyPaddingLength;
                    break;

                case BrJoinerType.Eui64:
                    if (payload.Length - offset < Eui64Length)
                        throw new BrPayloadException($"JOINER_TABLE entry {i} missing EUI-64.");
                    eui64 = [.. payload.Slice(offset, Eui64Length)];
                    offset += Eui64Length;
                    break;

                case BrJoinerType.Discerner:
                    if (offset >= payload.Length)
                        throw new BrPayloadException($"JOINER_TABLE entry {i} missing discerner length.");
                    
                    var bitLength = payload[offset++];
                    if (bitLength is < 1 or > 64)
                        throw new BrPayloadException($"JOINER_TABLE entry {i} invalid discerner length {bitLength} bits.");
                    
                    var discernerBytes = (bitLength + 7) / 8;
                    if (payload.Length - offset < discernerBytes)
                        throw new BrPayloadException($"JOINER_TABLE entry {i} discerner value exceeds payload.");
                    
                    discerner = new BrJoinerDiscerner(bitLength, [.. payload.Slice(offset, discernerBytes)]);
                    offset += discernerBytes;
                    break;

                default:
                    throw new BrPayloadException($"JOINER_TABLE entry {i} unknown type 0x{(byte)type:X2}.");
            }

            if (offset >= payload.Length)
                throw new BrPayloadException($"JOINER_TABLE entry {i} missing PSKD length.");
            
            var pskdLength = payload[offset++];
            if (pskdLength > payload.Length - offset)
                throw new BrPayloadException($"JOINER_TABLE entry {i} PSKD exceeds payload.");
            
            var pskd = Encoding.UTF8.GetString(payload.Slice(offset, pskdLength));
            offset += pskdLength;

            if (payload.Length - offset < 4)
                throw new BrPayloadException($"JOINER_TABLE entry {i} missing expiration time.");
            
            var expirationTimeMs = BinaryPrimitives.ReadUInt32BigEndian(payload[offset..(offset + 4)]);
            offset += 4;

            entries.Add(new BrJoinerEntry(type, eui64, discerner, pskd, expirationTimeMs));
        }

        return entries;
    }
}
