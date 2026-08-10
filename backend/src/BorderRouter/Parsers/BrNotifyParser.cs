using System.Buffers.Binary;
using Namorix.Weave.BorderRouter.Exceptions;
using Namorix.Weave.BorderRouter.Models;

namespace Namorix.Weave.BorderRouter.Parsers;

public static class BrNotifyParser
{
    private const int MaskLength = 4;

    public static BrChangedMask Parse(ReadOnlySpan<byte> payload)
    {
        if (payload.Length < MaskLength)
            throw new BrPayloadException($"NOTIFY payload must be {MaskLength} bytes, got {payload.Length}.");

        return (BrChangedMask)BinaryPrimitives.ReadUInt32BigEndian(payload[..MaskLength]);
    }
}
