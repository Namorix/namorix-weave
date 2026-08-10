using Namorix.Weave.BorderRouter.Exceptions;
using Namorix.Weave.BorderRouter.Models;

namespace Namorix.Weave.BorderRouter.Parsers;

public static class BrMacParser
{
    private const int MacAddressLength = 8;

    public static BrMacAddress Parse(ReadOnlySpan<byte> payload)
    {
        if (payload.Length < MacAddressLength)
            throw new BrPayloadException($"MAC_ADDRESS payload must be {MacAddressLength} bytes, got {payload.Length}.");
        return new BrMacAddress(payload[..MacAddressLength].ToArray());
    }
}
