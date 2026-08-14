using System.Security.Cryptography;

namespace Namorix.Weave.Services;

public sealed class SecretProtector(string dataDir)
{
    private const string KeyFileName = "weave.key";

    private readonly byte[] _key = LoadKey(dataDir);

    public string Encrypt(byte[] plaintext)
    {
        var nonce = RandomNumberGenerator.GetBytes(12);
        var ciphertext = new byte[plaintext.Length];
        var tag = new byte[16];
        using var aes = new AesGcm(_key, 16);
        aes.Encrypt(nonce, plaintext, ciphertext, tag);
        return Convert.ToBase64String([.. nonce, .. ciphertext, .. tag]);
    }

    public byte[] Decrypt(string value)
    {
        var blob = Convert.FromBase64String(value);
        if (blob.Length < 12 + 16)
            throw new InvalidDataException("Invalid ciphertext.");
        var nonce = blob.AsSpan(0, 12);
        var ciphertext = blob.AsSpan(12, blob.Length - 12 - 16);
        var tag = blob.AsSpan(blob.Length - 16, 16);
        var plaintext = new byte[ciphertext.Length];
        using var aes = new AesGcm(_key, 16);
        aes.Decrypt(nonce, ciphertext, tag, plaintext);
        return plaintext;
    }

    private static byte[] LoadKey(string dataDir)
    {
        var path = Path.Combine(dataDir, KeyFileName);
        if (File.Exists(path))
            return File.ReadAllBytes(path);

        var key = RandomNumberGenerator.GetBytes(32);
        File.WriteAllBytes(path, key);
        try
        {
            // Key file must be owner-only: anyone reading it can decrypt NetworkKey.
            File.SetUnixFileMode(path, UnixFileMode.UserRead | UnixFileMode.UserWrite);
        }
        catch (PlatformNotSupportedException)
        {
        }
        return key;
    }
}
