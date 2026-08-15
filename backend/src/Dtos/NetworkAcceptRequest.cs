namespace Namorix.Weave.Dtos;

public sealed record ThreadDatasetInput(
    ushort PanId,
    byte[] ExtendedPanId,
    byte Channel,
    uint ChannelMask,
    string? NetworkName,
    byte[] MeshLocalPrefix,
    byte[] NetworkKey,
    byte[] Pskc,
    byte[] SecurityPolicy);

public sealed record NetworkAcceptRequest(
    string? Name,
    ThreadDatasetInput? Dataset);
