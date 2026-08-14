using Makaretu.Dns;

namespace Namorix.Weave.BorderRouter;

public sealed record BrEndpoint(string InstanceName, string Host, int Port);

public sealed class BrMdnsBrowser(string service, int framePort, ILogger<BrMdnsBrowser> logger) : IAsyncDisposable
{
    private const int SweepIntervalSec = 15;
    private const int ExpiryTtlSec = 60;

    private readonly object _gate = new();
    private readonly Dictionary<string, BrEndpoint> _known = new();
    private readonly Dictionary<string, DateTime> _lastSeen = new();

    private ServiceDiscovery? _discovery;
    private Task? _sweepTask;
    private CancellationTokenSource? _cts;

    public event Action<BrEndpoint>? BrFound;
    public event Action<BrEndpoint>? BrLost;

    public void Start(CancellationToken ct)
    {
        _cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        var mdns = new MulticastService { UseIpv6 = false };
        _discovery = new ServiceDiscovery(mdns);
        _discovery.ServiceInstanceDiscovered += OnInstanceDiscovered;
        _discovery.ServiceInstanceShutdown += OnInstanceShutdown;
        mdns.Start();
        _discovery.QueryServiceInstances(service);
        _sweepTask = Task.Run(() => SweepLoopAsync(_cts.Token), CancellationToken.None);
    }

    private void OnInstanceDiscovered(object? sender, ServiceInstanceDiscoveryEventArgs e)
    {
        var ep = ResolveEndpoint(e);
        if (ep is null)
            return;

        bool isNew;
        lock (_gate)
        {
            _lastSeen[ep.InstanceName] = DateTime.UtcNow;
            isNew = !_known.ContainsKey(ep.InstanceName);
            if (isNew)
                _known[ep.InstanceName] = ep;
        }

        if (!isNew)
            return;

        logger.LogInformation("mDNS BR found: {Instance} at {Host}:{Port}.", ep.InstanceName, ep.Host, ep.Port);
        BrFound?.Invoke(ep);
    }

    private void OnInstanceShutdown(object? sender, ServiceInstanceShutdownEventArgs e)
    {
        var instance = e.ServiceInstanceName.ToString();
        BrEndpoint? ep;
        lock (_gate)
        {
            if (!_known.Remove(instance, out ep))
                return;
            _lastSeen.Remove(instance);
        }

        logger.LogInformation("mDNS BR bye: {Instance}.", instance);
        BrLost?.Invoke(ep);
    }

    private BrEndpoint? ResolveEndpoint(ServiceInstanceDiscoveryEventArgs e)
    {
        var answers = e.Message.Answers;
        var srv = answers.OfType<SRVRecord>().FirstOrDefault(r => r.Name == e.ServiceInstanceName);
        if (srv is null)
            return null;

        var ip = answers.OfType<ARecord>().FirstOrDefault(a => a.Name == srv.Target)?.Address
                 ?? answers.OfType<AAAARecord>().FirstOrDefault(a => a.Name == srv.Target)?.Address;
        if (ip is null)
            return null;

        var port = srv.Port != 0 ? (int)srv.Port : framePort;
        return new BrEndpoint(e.ServiceInstanceName.ToString(), ip.ToString(), port);
    }

    private async Task SweepLoopAsync(CancellationToken ct)
    {
        using var timer = new PeriodicTimer(TimeSpan.FromSeconds(SweepIntervalSec));
        while (!ct.IsCancellationRequested)
        {
            try
            {
                await timer.WaitForNextTickAsync(ct);
            }
            catch (OperationCanceledException)
            {
                break;
            }

            _discovery?.QueryServiceInstances(service);
            ExpireStale();
        }
    }

    private void ExpireStale()
    {
        var cutoff = DateTime.UtcNow - TimeSpan.FromSeconds(ExpiryTtlSec);
        List<BrEndpoint> lost = [];
        lock (_gate)
        {
            foreach (var (instance, seen) in _lastSeen.ToArray())
            {
                if (seen > cutoff)
                    continue;
                _lastSeen.Remove(instance);
                if (_known.Remove(instance, out var ep))
                    lost.Add(ep);
            }
        }

        foreach (var ep in lost)
        {
            logger.LogInformation("mDNS BR expired: {Instance}.", ep.InstanceName);
            BrLost?.Invoke(ep);
        }
    }

    public async ValueTask DisposeAsync()
    {
        _cts?.Cancel();
        if (_sweepTask is not null)
        {
            try
            {
                await _sweepTask;
            }
            catch
            {
            }
        }

        if (_discovery is not null)
        {
            _discovery.ServiceInstanceDiscovered -= OnInstanceDiscovered;
            _discovery.ServiceInstanceShutdown -= OnInstanceShutdown;
            _discovery.Dispose();
        }

        _cts?.Dispose();
    }
}
