using Microsoft.AspNetCore.DataProtection;

namespace Namorix.Weave.Services;

public sealed class WeaveSecretProtector(IDataProtectionProvider provider)
{
    private readonly IDataProtector _protector = provider.CreateProtector("Weave.ThreadDataset");
    private const string Magic = "CfDJ8";   // DataProtection magic prefix

    public string? Protect(string? value)
    {
        if (string.IsNullOrEmpty(value)) return value;
        // Idempotent: re-protecting an already-protected blob would double-encrypt.
        value = value.StartsWith(Magic, StringComparison.Ordinal) ? Unprotect(value) : value;
        return string.IsNullOrEmpty(value) ? value : _protector.Protect(value);
    }

    public string? Unprotect(string? value) =>
        string.IsNullOrEmpty(value) || !value.StartsWith(Magic, StringComparison.Ordinal)
            ? value          // Legacy plaintext -> keep as-is without breaking
            : _protector.Unprotect(value);
}
