using System.Runtime.InteropServices;

namespace Namorix.Weave.BorderRouter.Frame;

public static class FrameCodec
{
    private const byte SoF = 0xAA;
    private const byte EoF = 0x55;
    private const int MaxPayload = 2048;
    private const int MinFrameLength = 7;

    public static byte[] Encode(BrFrame frame)
    {
        ArgumentNullException.ThrowIfNull(frame);

        var payload = frame.Payload;
        if (payload.Length > MaxPayload)
            throw new ArgumentOutOfRangeException(nameof(frame), $"Payload exceeds {MaxPayload} bytes.");

        var len = payload.Length;
        var buffer = new byte[len + 7];

        buffer[0] = SoF;
        buffer[1] = frame.FrameId;
        buffer[2] = frame.Command;
        buffer[3] = (byte)(len >> 8);
        buffer[4] = (byte)(len & 0xFF);
        payload.CopyTo(buffer, 5);

        var crcInput = buffer.AsSpan(1, len + 4);
        buffer[5 + len] = Crc8Maxim.Compute(crcInput);
        buffer[6 + len] = EoF;

        return buffer;
    }

    public static bool TryParse(ReadOnlySpan<byte> data, out BrFrame? frame)
    {
        frame = null;

        if (data.Length < MinFrameLength || data[0] != SoF)
            return false;

        var len = (data[3] << 8) | data[4];
        if (len > MaxPayload)
            return false;

        var total = len + MinFrameLength;
        if (data.Length < total)
            return false;

        if (data[len + 6] != EoF)
            return false;

        var crc = Crc8Maxim.Compute(data.Slice(1, len + 4));
        if (crc != data[len + 5])
            return false;

        frame = new BrFrame(data[1], data[2], [.. data.Slice(5, len)]);
        return true;
    }

    public static bool TryParseNext(List<byte> buffer, out BrFrame? frame)
    {
        frame = null;

        while (buffer.Count > 0)
        {
            if (buffer[0] != SoF)
            {
                buffer.RemoveAt(0);
                continue;
            }

            if (buffer.Count < MinFrameLength)
                return false;

            var len = (buffer[3] << 8) | buffer[4];
            if (len > MaxPayload)
            {
                buffer.RemoveAt(0);
                continue;
            }

            if (buffer.Count < len + MinFrameLength)
                return false;

            var total = len + MinFrameLength;
            if (TryParse(CollectionsMarshal.AsSpan(buffer), out frame))
            {
                buffer.RemoveRange(0, total);
                return true;
            }

            buffer.RemoveAt(0);
        }

        return false;
    }
}
