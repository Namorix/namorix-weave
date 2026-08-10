namespace Namorix.Weave.BorderRouter.Exceptions;

public enum BrNackCode : byte
{
    InvalidCommand = 0x01,
    NotReady = 0x02,
    Timeout = 0x03,
    InvalidParameter = 0x04,
    Busy = 0x05,
}

public sealed class BrNackException(BrNackCode code) : Exception($"BR NACK: {code} (0x{(byte)code:X2})")
{
    public BrNackCode Code { get; } = code;
}
