namespace Namorix.Weave.Models;

public sealed class Network
{
    public int Id { get; set; }

    public Protocol Protocol { get; set; }

    public string? Name { get; set; }

    public string? Host { get; set; }

    public NetworkStatus Status { get; set; } = NetworkStatus.Pending;

    public string? Eui64 { get; set; }

    public string? PublicKey { get; set; }

    public DateTime? FirstSeenAt { get; set; }
    public DateTime? AcceptedAt { get; set; }
    public DateTime? RejectedAt { get; set; }
    public DateTime CreatedAt { get; set; }

    public BrThreadDataset? ThreadDataset { get; set; }
}
