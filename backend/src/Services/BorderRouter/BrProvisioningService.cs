using System.Text;
using Microsoft.EntityFrameworkCore;
using Namorix.Weave.BorderRouter;
using Namorix.Weave.Dtos;
using Namorix.Weave.Models;
using Namorix.Weave.Persistence;

namespace Namorix.Weave.Services.BorderRouter;

public sealed class BrProvisioningService(
    TimeSpan requestTimeout,
    IDbContextFactory<WeaveDbContext> dbFactory,
    WeaveSecretProtector secretProtector,
    ILogger<BrProvisioningService> logger)
{
    public async Task<Network?> RegisterConnectionAsync(BrTcpClient connection, CancellationToken ct)
    {
        var client = new BrCommandClient(connection, requestTimeout);

        string eui64;
        try
        {
            var mac = await client.GetMacAsync(ct);
            if (mac.Length != 8)
            {
                logger.LogWarning("BR {Host} returned invalid MAC length {Len}.", connection.Host, mac.Length);
                return null;
            }
            eui64 = Convert.ToHexString(mac).ToLowerInvariant();
        }
        catch (Exception ex)
        {
            logger.LogDebug(ex, "BR handshake failed for {Host} (MAC_ADDRESS).", connection.Host);
            return null;
        }

        await using var db = await dbFactory.CreateDbContextAsync(ct);
        var network = await GetOrCreatePendingAsync(db, eui64, connection.Host, ct);
        logger.LogInformation("BR {Eui64} from {Host} -> {Status} (id={Id}).", eui64, connection.Host, network.Status, network.Id);
        return network;
    }

    private static async Task<Network> GetOrCreatePendingAsync(WeaveDbContext db, string eui64, string host, CancellationToken ct)
    {
        var existing = await db.Networks.SingleOrDefaultAsync(n => n.Eui64 == eui64, ct);
        if (existing is not null)
            return existing;

        var now = DateTime.UtcNow;
        var network = new Network
        {
            Protocol = Protocol.Thread,
            Host = host,
            Status = NetworkStatus.Pending,
            Eui64 = eui64,
            FirstSeenAt = now,
            CreatedAt = now,
        };

        db.Networks.Add(network);
        await db.SaveChangesAsync(ct);
        return network;
    }

    public async Task<Network?> AcceptAsync(int networkId, NetworkAcceptRequest request, CancellationToken ct)
    {
        if (request.Dataset is not null)
            ValidateDataset(request.Dataset);

        await using var db = await dbFactory.CreateDbContextAsync(ct);
        var network = await db.Networks.SingleOrDefaultAsync(n => n.Id == networkId, ct);
        if (network is null || network.Status != NetworkStatus.Pending)
            return null;

        network.Name = request.Name;
        network.Status = NetworkStatus.Connected;
        network.AcceptedAt = DateTime.UtcNow;

        if (request.Dataset is not null)
        {
            network.ThreadDataset = new BrThreadDataset
            {
                NetworkId = network.Id,
                PanId = request.Dataset.PanId,
                ExtendedPanId = request.Dataset.ExtendedPanId,
                Channel = request.Dataset.Channel,
                ChannelMask = request.Dataset.ChannelMask,
                NetworkName = request.Dataset.NetworkName,
                MeshLocalPrefix = request.Dataset.MeshLocalPrefix,
                NetworkKeyEncrypted = secretProtector.Protect(Convert.ToBase64String(request.Dataset.NetworkKey)),
                Pskc = request.Dataset.Pskc,
                SecurityPolicy = request.Dataset.SecurityPolicy,
            };
        }

        await db.SaveChangesAsync(ct);
        return network;
    }

    public async Task<Network?> RejectAsync(int networkId, CancellationToken ct)
    {
        await using var db = await dbFactory.CreateDbContextAsync(ct);
        var network = await db.Networks.SingleOrDefaultAsync(n => n.Id == networkId, ct);
        if (network is null || network.Status != NetworkStatus.Pending)
            return null;

        network.Status = NetworkStatus.Rejected;
        network.RejectedAt = DateTime.UtcNow;
        await db.SaveChangesAsync(ct);
        return network;
    }

    public async Task<Network?> MarkOfflineAsync(int networkId, CancellationToken ct)
    {
        await using var db = await dbFactory.CreateDbContextAsync(ct);
        var network = await db.Networks.SingleOrDefaultAsync(n => n.Id == networkId, ct);
        if (network is null || network.Status != NetworkStatus.Connected)
            return null;

        network.Status = NetworkStatus.Offline;
        await db.SaveChangesAsync(ct);
        return network;
    }

    public async Task<Network?> GetNetworkAsync(int id, CancellationToken ct)
    {
        await using var db = await dbFactory.CreateDbContextAsync(ct);
        return await db.Networks.SingleOrDefaultAsync(n => n.Id == id, ct);
    }

    public async Task<IReadOnlyList<Network>> ListNetworksAsync(CancellationToken ct)
    {
        await using var db = await dbFactory.CreateDbContextAsync(ct);
        return await db.Networks.OrderByDescending(n => n.FirstSeenAt).ToListAsync(ct);
    }

    private static void ValidateDataset(ThreadDatasetInput dataset)
    {
        if (dataset.ExtendedPanId.Length != 8)
            throw new ArgumentException("Extended PAN ID must be 8 bytes.");
        if (dataset.NetworkKey.Length != 16)
            throw new ArgumentException("Network key must be 16 bytes.");
        if (dataset.MeshLocalPrefix.Length != 8)
            throw new ArgumentException("Mesh-local prefix must be 8 bytes.");
        if (dataset.Pskc.Length != 16)
            throw new ArgumentException("PSKc must be 16 bytes.");
        if (dataset.SecurityPolicy.Length != 2)
            throw new ArgumentException("Security policy must be 2 bytes.");
        if (dataset.NetworkName is not null && Encoding.UTF8.GetByteCount(dataset.NetworkName) > 16)
            throw new ArgumentException("Network name must not exceed 16 bytes.");
    }
}
