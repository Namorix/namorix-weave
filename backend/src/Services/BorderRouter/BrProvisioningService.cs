using Microsoft.EntityFrameworkCore;
using Namorix.Weave.BorderRouter;
using Namorix.Weave.Models;
using Namorix.Weave.Persistence;

namespace Namorix.Weave.Services.BorderRouter;

public sealed class BrProvisioningService(
    TimeSpan requestTimeout,
    IDbContextFactory<WeaveDbContext> dbFactory,
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
}
