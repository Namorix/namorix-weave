namespace Namorix.Weave.Models;

public sealed class BrThreadDataset
{
    public int NetworkId { get; set; }

    public ushort PanId { get; set; }

    public byte[] ExtendedPanId { get; set; } = [];

    public byte Channel { get; set; }

    public uint ChannelMask { get; set; }

    public string? NetworkName { get; set; }

    public byte[] MeshLocalPrefix { get; set; } = [];

    public string? NetworkKeyEncrypted { get; set; }

    public byte[] Pskc { get; set; } = [];

    public byte[] SecurityPolicy { get; set; } = [];

    public Network Network { get; set; } = null!;
}
