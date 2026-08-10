namespace Namorix.Weave.BorderRouter.Frame;

public static class Crc8Maxim
{
    private const byte Polynomial = 0x31;

    public static byte Compute(ReadOnlySpan<byte> data)
    {
        byte crc = 0x00;
        foreach (var b in data)
        {
            crc ^= b;
            for (var i = 0; i < 8; i++)
            {
                crc = (crc & 0x80) != 0
                    ? (byte)((crc << 1) ^ Polynomial)
                    : (byte)(crc << 1);
            }
        }
        return crc;
    }
}
