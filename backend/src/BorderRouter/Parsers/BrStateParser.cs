using Namorix.Weave.BorderRouter.Exceptions;
using Namorix.Weave.BorderRouter.Models;

namespace Namorix.Weave.BorderRouter.Parsers;

public static class BrStateParser
{
    private const int StateLength = 1;

    public static BrRole Parse(ReadOnlySpan<byte> payload)
    {
        if (payload.Length < StateLength)
            throw new BrPayloadException($"STATE payload must be {StateLength} byte, got {payload.Length}.");
        return (BrRole)payload[0];
    }
}
