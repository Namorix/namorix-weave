namespace Namorix.Weave.BorderRouter.Models;

public enum BrJoinerType : byte
{
    Any = 0,
    Eui64 = 1,
    Discerner = 2,
}

public readonly record struct BrJoinerDiscerner(int BitLength, byte[] Value);

public sealed record BrJoinerEntry(
    BrJoinerType Type,
    byte[]? Eui64,
    BrJoinerDiscerner? Discerner,
    string Pskd,
    uint ExpirationTimeMs);
