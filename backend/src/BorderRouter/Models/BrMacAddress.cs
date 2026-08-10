namespace Namorix.Weave.BorderRouter.Models;

public readonly record struct BrMacAddress(byte[] Value)
{
    public override string ToString() => string.Join(':', Value.Select(b => b.ToString("X2")));
}
