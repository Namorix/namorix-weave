using System.Buffers.Binary;
using System.Globalization;
using System.Text;
using Namorix.Weave.BorderRouter.Models;
using Namorix.Weave.Dtos;
using Namorix.Weave.Models;

namespace Namorix.Weave.BorderRouter.Dtos;

public static class BrDtoMapper
{
    private static readonly DateTimeOffset ThreadEpoch = new(2000, 1, 1, 0, 0, 0, TimeSpan.Zero);

    private const string Base32Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

    public static string RoleToString(BrRole role) => role switch
    {
        BrRole.Leader => "leader",
        BrRole.Router => "router",
        BrRole.Child => "child",
        BrRole.Detached => "detached",
        _ => "disabled",
    };

    public static BrStateDto ToState(BrRole role, string? ipAddress, string? threadVersion, BrConnectionDto connection) =>
        new(connection, RoleToString(role), ipAddress, threadVersion);

    public static NetworkDto ToNetwork(Network network) =>
        new(
            network.Id,
            network.Protocol.ToString(),
            network.Name,
            network.Host,
            network.Status.ToString(),
            network.Eui64,
            network.PublicKey,
            network.FirstSeenAt,
            network.AcceptedAt,
            network.RejectedAt);

    public static BrDatasetDto ToDataset(BrActiveDataset dataset) =>
        new(
            ActiveTimestampFrom(dataset),
            NetworkNameFrom(dataset),
            ChannelFrom(dataset),
            ChannelMaskFrom(dataset),
            ColonHexTlv(dataset, MeshCopTlvType.ExtendedPanId),
            MeshLocalPrefixFrom(dataset),
            HexTlv(dataset, MeshCopTlvType.NetworkKey),
            PanIdFrom(dataset),
            PskcFrom(dataset),
            SecurityPolicyFrom(dataset));

    public static BrHealthDto ToHealth(BrHealth health) =>
        new(
            health.FreeHeapBytes,
            health.MinFreeHeapBytes,
            health.UptimeMs,
            health.MleDetachCount,
            health.Tasks.Select(t => new BrHealthTaskDto(t.Name, t.HighWaterMarkBytes, t.StackSizeBytes)).ToArray());

    public static BrRouterEntryDto ToRouterEntry(BrRouterEntry entry)
    {
        var rloc = $"0x{entry.Rloc16:x4}";
        return new BrRouterEntryDto(rloc, entry.RouterId, rloc, ColonHexLower(entry.ExtAddress),
            entry.LinkQualityIn, entry.LinkQualityOut, entry.AgeSeconds);
    }

    public static BrChildEntryDto ToChildEntry(BrChildEntry entry)
    {
        var rloc = $"0x{entry.Rloc16:x4}";
        return new BrChildEntryDto(rloc, entry.ChildId, rloc, ColonHexLower(entry.ExtAddress),
            entry.LinkQualityIn, entry.AverageRssi, entry.FullThreadDevice, entry.RxOnWhenIdle, entry.AgeSeconds);
    }

    public static BrJoinerEntryDto ToJoinerEntry(BrJoinerEntry entry) => entry.Type switch
    {
        BrJoinerType.Eui64 when entry.Eui64 is not null =>
            Joiner("eui64", entry.Eui64, entry.Pskd, entry.ExpirationTimeMs),
        BrJoinerType.Discerner when entry.Discerner is { Value: { } discernerValue } =>
            Joiner("discerner", discernerValue, entry.Pskd, entry.ExpirationTimeMs),
        _ => new BrJoinerEntryDto("any", "any", "—", entry.Pskd, entry.ExpirationTimeMs),
    };

    private static BrJoinerEntryDto Joiner(string type, byte[] value, string pskd, uint expirationTimeMs)
    {
        var hex = Convert.ToHexStringLower(value);
        return new BrJoinerEntryDto($"{type}:{hex}", type, ColonHexLower(value), pskd, expirationTimeMs);
    }

    private static string? ActiveTimestampFrom(BrActiveDataset dataset)
    {
        if (dataset.FindTlv(MeshCopTlvType.ActiveTimestamp) is not { Length: >= 6 } value)
            return null;

        var seconds = BinaryPrimitives.ReadUInt32BigEndian(value[..4]);
        return ThreadEpoch.AddSeconds(seconds).UtcDateTime
            .ToString("yyyy-MM-dd'T'HH:mm:ss'Z'", CultureInfo.InvariantCulture);
    }

    private static string? NetworkNameFrom(BrActiveDataset dataset)
    {
        var value = dataset.FindTlv(MeshCopTlvType.NetworkName);
        return value is null ? null : Encoding.UTF8.GetString(value);
    }

    private static int? ChannelFrom(BrActiveDataset dataset)
    {
        if (dataset.FindTlv(MeshCopTlvType.Channel) is not { Length: >= 3 } value)
            return null;

        return BinaryPrimitives.ReadUInt16BigEndian(value[1..3]);
    }

    private static string? ChannelMaskFrom(BrActiveDataset dataset)
    {
        var value = dataset.FindTlv(MeshCopTlvType.ChannelMask);
        return value is null ? null : "0x" + Convert.ToHexString(value);
    }

    private static string? PanIdFrom(BrActiveDataset dataset)
    {
        if (dataset.FindTlv(MeshCopTlvType.PanId) is not { Length: >= 2 } value)
            return null;

        return "0x" + BinaryPrimitives.ReadUInt16BigEndian(value[..2]).ToString("x4", CultureInfo.InvariantCulture);
    }

    private static string? MeshLocalPrefixFrom(BrActiveDataset dataset)
    {
        if (dataset.FindTlv(MeshCopTlvType.MeshLocalPrefix) is not { Length: >= 8 } value)
            return null;

        var hextets = new string[4];
        for (var i = 0; i < 4; i++)
            hextets[i] = ((value[2 * i] << 8) | value[2 * i + 1]).ToString("x", CultureInfo.InvariantCulture);

        return string.Join(':', hextets) + "::/64";
    }

    private static string? PskcFrom(BrActiveDataset dataset)
    {
        var value = dataset.FindTlv(MeshCopTlvType.Pskc);
        return value is null ? null : Base32Encode(value);
    }

    private static string? SecurityPolicyFrom(BrActiveDataset dataset)
    {
        if (dataset.FindTlv(MeshCopTlvType.SecurityPolicy) is not { Length: >= 2 } value)
            return null;

        return $"0x{value[0]:X2} / {value[1]}";
    }

    private static string? ColonHexTlv(BrActiveDataset dataset, byte type)
    {
        var value = dataset.FindTlv(type);
        return value is null ? null : ColonHexLower(value);
    }

    private static string? HexTlv(BrActiveDataset dataset, byte type)
    {
        var value = dataset.FindTlv(type);
        return value is null ? null : Convert.ToHexStringLower(value);
    }

    private static string ColonHexLower(ReadOnlySpan<byte> value) =>
        string.Join(':', Convert.ToHexStringLower(value).Chunk(2).Select(chunk => new string(chunk)));

    private static string Base32Encode(ReadOnlySpan<byte> data)
    {
        var builder = new StringBuilder((data.Length * 8 + 4) / 5);
        var buffer = 0;
        var bits = 0;

        foreach (var b in data)
        {
            buffer = (buffer << 8) | b;
            bits += 8;
            while (bits >= 5)
            {
                builder.Append(Base32Alphabet[(buffer >> (bits - 5)) & 0x1F]);
                bits -= 5;
            }
            buffer &= (1 << bits) - 1;
        }

        if (bits > 0)
            builder.Append(Base32Alphabet[(buffer << (5 - bits)) & 0x1F]);

        return builder.ToString();
    }
}
