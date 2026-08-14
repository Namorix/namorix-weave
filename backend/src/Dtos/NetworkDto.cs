namespace Namorix.Weave.Dtos;

public sealed record NetworkDto(
    int Id,
    string Protocol,
    string? Name,
    string? Host,
    string Status,
    string? Eui64,
    string? PublicKey,
    DateTime? FirstSeenAt,
    DateTime? AcceptedAt,
    DateTime? RejectedAt);
