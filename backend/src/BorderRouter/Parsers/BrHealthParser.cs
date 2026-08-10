using System.Buffers.Binary;
using System.Text;
using Namorix.Weave.BorderRouter.Exceptions;
using Namorix.Weave.BorderRouter.Models;

namespace Namorix.Weave.BorderRouter.Parsers;

public static class BrHealthParser
{
    private const int PrefixLength = 16;
    private const int TaskNameType = 0x01;
    private const int HighWaterMarkType = 0x02;
    private const int StackSizeType = 0x03;

    public static BrHealth Parse(ReadOnlySpan<byte> payload)
    {
        if (payload.Length < PrefixLength)
            throw new BrPayloadException($"BR_HEALTH payload must be at least {PrefixLength} bytes, got {payload.Length}.");

        var freeHeap = BinaryPrimitives.ReadUInt32BigEndian(payload[0..4]);
        var minFreeHeap = BinaryPrimitives.ReadUInt32BigEndian(payload[4..8]);
        var uptimeMs = BinaryPrimitives.ReadUInt32BigEndian(payload[8..12]);
        var mleDetachCount = BinaryPrimitives.ReadUInt32BigEndian(payload[12..16]);

        return new BrHealth(freeHeap, minFreeHeap, uptimeMs, mleDetachCount, ParseTasks(payload[16..]));
    }

    private static List<BrHealthTask> ParseTasks(ReadOnlySpan<byte> data)
    {
        var tasks = new List<BrHealthTask>();
        var offset = 0;

        while (offset < data.Length)
        {
            if (!TryReadTlv(data, ref offset, out var nameType, out var nameValue) || nameType != TaskNameType)
                throw new BrPayloadException("BR_HEALTH task TLV missing or malformed.");

            if (!TryReadTlv(data, ref offset, out var hwmType, out var hwmValue) || hwmType != HighWaterMarkType || hwmValue.Length != 4)
                throw new BrPayloadException("BR_HEALTH task missing high water mark TLV.");

            if (!TryReadTlv(data, ref offset, out var stackType, out var stackValue) || stackType != StackSizeType || stackValue.Length != 4)
                throw new BrPayloadException("BR_HEALTH task missing stack size TLV.");

            tasks.Add(new BrHealthTask(
                Encoding.UTF8.GetString(nameValue),
                BinaryPrimitives.ReadUInt32BigEndian(hwmValue),
                BinaryPrimitives.ReadUInt32BigEndian(stackValue)));
        }

        return tasks;
    }

    private static bool TryReadTlv(ReadOnlySpan<byte> data, ref int offset, out byte type, out ReadOnlySpan<byte> value)
    {
        type = 0;
        value = default;
        if (offset + 2 > data.Length)
            return false;

        type = data[offset];
        var length = data[offset + 1];
        offset += 2;
        if (length > data.Length - offset)
            return false;

        value = data.Slice(offset, length);
        offset += length;
        return true;
    }
}
