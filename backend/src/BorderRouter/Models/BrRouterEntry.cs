namespace Namorix.Weave.BorderRouter.Models;

public sealed record BrRouterEntry(
    byte RouterId,
    ushort Rloc16,
    byte[] ExtAddress,
    byte LinkQualityIn,
    byte LinkQualityOut,
    ushort AgeSeconds);
