namespace Namorix.Weave.BorderRouter.Dtos;

public sealed record BrConnectionDto(bool IsConnected, string? Host, int? Port);

public sealed record BrStateDto(
    BrConnectionDto Connection,
    string Role,
    string? IpAddress,
    string? ThreadVersion);

public sealed record BrHealthTaskDto(string Name, uint HighWaterMarkBytes, uint StackSizeBytes);

public sealed record BrHealthDto(
    uint FreeHeapBytes,
    uint MinFreeHeapBytes,
    uint UptimeMs,
    uint MleDetachCount,
    IReadOnlyList<BrHealthTaskDto> Tasks);

public sealed record BrDatasetDto(
    string? ActiveTimestamp,
    string? NetworkName,
    int? Channel,
    string? ChannelMask,
    string? ExtendedPanId,
    string? MeshLocalPrefix,
    string? NetworkKey,
    string? PanId,
    string? Pskc,
    string? SecurityPolicy);

public sealed record BrRouterEntryDto(
    string Id,
    byte RouterId,
    string Rloc16,
    string ExtAddress,
    byte LinkQualityIn,
    byte LinkQualityOut,
    ushort Age);

public sealed record BrChildEntryDto(
    string Id,
    byte ChildId,
    string Rloc16,
    string ExtAddress,
    byte LinkQualityIn,
    int AverageRssi,
    bool FullThreadDevice,
    bool RxOnWhenIdle,
    ushort Age);

public sealed record BrJoinerEntryDto(
    string Id,
    string Type,
    string SharedId,
    string Pskd,
    uint ExpirationTime);
