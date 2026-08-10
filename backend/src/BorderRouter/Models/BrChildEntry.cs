namespace Namorix.Weave.BorderRouter.Models;

public sealed record BrChildEntry(
    byte ChildId,
    ushort Rloc16,
    byte[] ExtAddress,
    byte LinkQualityIn,
    sbyte AverageRssi,
    bool FullThreadDevice,
    bool RxOnWhenIdle,
    ushort AgeSeconds);
