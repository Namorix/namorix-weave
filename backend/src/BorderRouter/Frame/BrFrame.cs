namespace Namorix.Weave.BorderRouter.Frame;

public sealed record BrFrame(byte FrameId, byte Command, byte[] Payload)
{
    public int Length => Payload.Length;
}
