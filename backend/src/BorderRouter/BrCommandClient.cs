using System.Text;
using Namorix.Weave.BorderRouter.Exceptions;
using Namorix.Weave.BorderRouter.Frame;

namespace Namorix.Weave.BorderRouter;

public sealed class BrCommandClient(BrTcpClient transport, TimeSpan? requestTimeout = null)
{
    private const int MaxNetworkNameLength = 16;
    private const int ExtendedPanIdLength = 8;
    private const int NetworkKeyLength = 16;
    private const int JoinerEui64Length = 8;
    private const int MinPskdLength = 6;
    private const int MaxPskdLength = 32;
    private const int SrpIpv6Length = 16;
    private const byte FactoryResetConfirmByte = 0xAA;

    private readonly TimeSpan _timeout = requestTimeout ?? TimeSpan.FromSeconds(3);

    public async Task<byte[]> RequestAsync(BrCommand command, byte[] data, CancellationToken ct = default)
    {
        var frame = await RequestFrameAsync(command, data, ct);
        return frame.Payload;
    }

    private async Task<BrFrame> RequestFrameAsync(BrCommand command, byte[] data, CancellationToken ct)
    {
        var response = await transport.RequestAsync((byte)command, data, _timeout, ct);
        return response.Command switch
        {
            (byte)BrCommand.Ack => response,
            (byte)BrCommand.Nack => throw new BrNackException(NackCodeFrom(response.Payload)),
            _ => throw new InvalidDataException($"Unexpected BR response command 0x{response.Command:X2}."),
        };
    }

    private static BrNackCode NackCodeFrom(byte[] payload) =>
        payload.Length > 0
            ? (BrNackCode)payload[0]
            : throw new InvalidDataException("NACK payload missing error code.");

    public async Task<byte[]> GetStateAsync(CancellationToken ct = default) =>
        await RequestAsync(BrCommand.State, [], ct);

    public async Task<byte[]> GetDatasetActiveAsync(CancellationToken ct = default) =>
        await RequestAsync(BrCommand.DatasetActive, [], ct);

    public async Task<byte[]> GetIpAddrAsync(CancellationToken ct = default)
    {
        var response = await RequestFrameAsync(BrCommand.IpAddr, [], ct);
        await transport.SendFrameAsync(new BrFrame(response.FrameId, (byte)BrCommand.Ack, []), ct);
        return response.Payload;
    }

    public async Task<byte[]> GetMacAsync(CancellationToken ct = default) =>
        await RequestAsync(BrCommand.MacAddress, [], ct);

    public async Task<byte[]> GetBrHealthAsync(CancellationToken ct = default) =>
        await RequestAsync(BrCommand.BrHealth, [], ct);

    public async Task<byte[]> GetRouterTableAsync(CancellationToken ct = default) =>
        await RequestAsync(BrCommand.RouterTable, [], ct);

    public async Task<byte[]> GetChildTableAsync(CancellationToken ct = default) =>
        await RequestAsync(BrCommand.ChildTable, [], ct);

    public async Task<byte[]> GetJoinerTableAsync(CancellationToken ct = default) =>
        await RequestAsync(BrCommand.JoinerTable, [], ct);

    public async Task SetPanIdAsync(ushort panId, CancellationToken ct = default) =>
        await RequestAsync(BrCommand.SetPanId, [(byte)(panId >> 8), (byte)panId], ct);

    public async Task SetChannelAsync(byte channel, CancellationToken ct = default) =>
        await RequestAsync(BrCommand.SetChannel, [channel], ct);

    public async Task SetNetworkNameAsync(string name, CancellationToken ct = default)
    {
        var nameBytes = Encoding.UTF8.GetBytes(name);
        if (nameBytes.Length > MaxNetworkNameLength)
            throw new ArgumentException($"Network name exceeds {MaxNetworkNameLength} bytes.", nameof(name));
        await RequestAsync(BrCommand.SetNetworkName, nameBytes, ct);
    }

    public async Task SetExtendedPanIdAsync(byte[] extendedPanId, CancellationToken ct = default)
    {
        if (extendedPanId.Length != ExtendedPanIdLength)
            throw new ArgumentException($"Extended PAN ID must be {ExtendedPanIdLength} bytes.", nameof(extendedPanId));
        await RequestAsync(BrCommand.SetExtendedPanId, extendedPanId, ct);
    }

    public async Task SetNetworkKeyAsync(byte[] networkKey, CancellationToken ct = default)
    {
        if (networkKey.Length != NetworkKeyLength)
            throw new ArgumentException($"Network key must be {NetworkKeyLength} bytes.", nameof(networkKey));
        await RequestAsync(BrCommand.SetNetworkKey, networkKey, ct);
    }

    public async Task StartThreadAsync(CancellationToken ct = default) =>
        await RequestAsync(BrCommand.ThreadStart, [], ct);

    public async Task StopThreadAsync(CancellationToken ct = default) =>
        await RequestAsync(BrCommand.ThreadStop, [], ct);

    public async Task<byte[]> GetThreadVersionAsync(CancellationToken ct = default) =>
        await RequestAsync(BrCommand.ThreadVersion, [], ct);

    public async Task AddJoinerAsync(byte[] eui64, string pskd, uint timeoutSeconds, CancellationToken ct = default)
    {
        if (eui64.Length != JoinerEui64Length)
            throw new ArgumentException($"EUI-64 must be {JoinerEui64Length} bytes.", nameof(eui64));

        var pskdBytes = Encoding.UTF8.GetBytes(pskd);
        if (pskdBytes.Length is < MinPskdLength or > MaxPskdLength)
            throw new ArgumentException($"PSKd must be {MinPskdLength}–{MaxPskdLength} characters.", nameof(pskd));

        var payload = new byte[JoinerEui64Length + 1 + pskdBytes.Length + sizeof(uint)];
        eui64.CopyTo(payload, 0);
        payload[JoinerEui64Length] = (byte)pskdBytes.Length;
        pskdBytes.CopyTo(payload, JoinerEui64Length + 1);
        
        var timeoutOffset = JoinerEui64Length + 1 + pskdBytes.Length;
        payload[timeoutOffset] = (byte)(timeoutSeconds >> 24);
        payload[timeoutOffset + 1] = (byte)(timeoutSeconds >> 16);
        payload[timeoutOffset + 2] = (byte)(timeoutSeconds >> 8);
        payload[timeoutOffset + 3] = (byte)timeoutSeconds;

        await RequestAsync(BrCommand.CommissionerJoiner, payload, ct);
    }

    public async Task RegisterSrpAsync(string hostname, byte[] ipv6Address, ushort port, CancellationToken ct = default)
    {
        if (ipv6Address.Length != SrpIpv6Length)
            throw new ArgumentException($"IPv6 address must be {SrpIpv6Length} bytes.", nameof(ipv6Address));

        var hostnameBytes = Encoding.UTF8.GetBytes(hostname);
        if (hostnameBytes.Length > byte.MaxValue)
            throw new ArgumentException("Hostname exceeds 255 bytes.", nameof(hostname));

        var payload = new byte[1 + hostnameBytes.Length + SrpIpv6Length + sizeof(ushort)];
        payload[0] = (byte)hostnameBytes.Length;
        hostnameBytes.CopyTo(payload, 1);
        
        var ipv6Offset = 1 + hostnameBytes.Length;
        ipv6Address.CopyTo(payload, ipv6Offset);
        payload[ipv6Offset + SrpIpv6Length] = (byte)(port >> 8);
        payload[ipv6Offset + SrpIpv6Length + 1] = (byte)port;

        await RequestAsync(BrCommand.SrpRegister, payload, ct);
    }

    public async Task ResetAsync(CancellationToken ct = default) =>
        await RequestAsync(BrCommand.Reset, [], ct);

    public async Task FactoryResetAsync(CancellationToken ct = default) =>
        await RequestAsync(BrCommand.Factory, [FactoryResetConfirmByte], ct);
}
