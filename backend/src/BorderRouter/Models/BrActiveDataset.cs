namespace Namorix.Weave.BorderRouter.Models;

public static class MeshCopTlvType
{
    public const byte Channel = 0x00;
    public const byte PanId = 0x01;
    public const byte ExtendedPanId = 0x02;
    public const byte NetworkName = 0x03;
    public const byte Pskc = 0x04;
    public const byte NetworkKey = 0x05;
    public const byte MeshLocalPrefix = 0x07;
    public const byte SecurityPolicy = 0x0c;
    public const byte ActiveTimestamp = 0x0e;
    public const byte ChannelMask = 0x35;
}

public sealed record BrActiveDataset(byte[] Raw)
{
    public byte[]? FindTlv(byte type)
    {
        var offset = 0;
        while (offset + 2 <= Raw.Length)
        {
            var tlvType = Raw[offset];
            var tlvLength = Raw[offset + 1];
            offset += 2;
            if (tlvLength > Raw.Length - offset)
                return null;
            if (tlvType == type)
                return [.. Raw.AsSpan(offset, tlvLength)];
            offset += tlvLength;
        }
        return null;
    }
}
