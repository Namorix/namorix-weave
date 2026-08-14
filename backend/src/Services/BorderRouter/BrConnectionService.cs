using System.Collections.Concurrent;
using System.Net;
using System.Text;
using Microsoft.AspNetCore.SignalR;
using Microsoft.Extensions.Options;
using Namorix.Weave.BorderRouter;
using Namorix.Weave.BorderRouter.Dtos;
using Namorix.Weave.BorderRouter.Frame;
using Namorix.Weave.BorderRouter.Models;
using Namorix.Weave.BorderRouter.Parsers;
using Namorix.Weave.Constants;
using Namorix.Weave.Hubs;
using Namorix.Weave.Models;

namespace Namorix.Weave.Services.BorderRouter;

public sealed class BrConnectionService(BrMdnsBrowser browser, BrProvisioningService provisioning,
    IHubContext<BrHub> hub, IOptions<BrOptions> options, ILogger<BrConnectionService> logger,
    ILogger<BrTcpClient> brClientLogger)
    : BackgroundService
{
    private const int HealthEveryPolls = 3;

    private sealed class ActiveBr
    {
        public required BrEndpoint Endpoint { get; init; }
        public required BrTcpClient Client { get; init; }
        public required BrCommandClient CommandClient { get; init; }
        public required int NetworkId { get; init; }
        public NetworkStatus Status { get; set; } = NetworkStatus.Pending;
        public BrRole? PublishedRole { get; set; }
    }

    private readonly BrOptions _options = options.Value;
    private readonly ConcurrentDictionary<string, BrTcpClient> _clients = new();
    private readonly ConcurrentDictionary<string, ActiveBr> _active = new();
    private readonly SemaphoreSlim _notifyGate = new(1, 1);

    private CancellationToken _stopping = CancellationToken.None;
    private int _pollCount;

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        _stopping = stoppingToken;
        browser.BrFound += OnBrFound;
        browser.BrLost += OnBrLost;
        browser.Start(stoppingToken);

        try
        {
            using var timer = new PeriodicTimer(TimeSpan.FromSeconds(_options.StatePollIntervalSec));
            while (!stoppingToken.IsCancellationRequested)
            {
                await timer.WaitForNextTickAsync(stoppingToken);
                await PollActiveAsync(stoppingToken);
            }
        }
        catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested)
        {
        }
        finally
        {
            browser.BrFound -= OnBrFound;
            browser.BrLost -= OnBrLost;
        }
    }

    private void OnBrFound(BrEndpoint ep)
    {
        var client = new BrTcpClient(ep.Host, ep.Port, brClientLogger);
        client.Connected += () => _ = OnClientConnectedAsync(ep, client);
        client.Disconnected += () => _ = OnClientDisconnectedAsync(ep);
        client.FrameReceived += frame => OnFrameReceived(ep, client, frame);
        if (!_clients.TryAdd(ep.InstanceName, client))
        {
            _ = client.DisposeAsync();
            return;
        }

        logger.LogInformation("Connecting to BR {Instance} at {Host}:{Port}.", ep.InstanceName, ep.Host, ep.Port);
        client.Start(_stopping);
    }

    private void OnBrLost(BrEndpoint ep)
    {
        if (!_clients.TryRemove(ep.InstanceName, out var client))
            return;

        _active.TryRemove(ep.InstanceName, out var br);
        _ = client.DisposeAsync();
        if (br is not null && br.Status == NetworkStatus.Connected)
            _ = SafePushAsync(ct => PushConnectionAsync(false, br, ct), CancellationToken.None);
        _ = PushNetworkListAsync(CancellationToken.None);
    }

    private async Task OnClientConnectedAsync(BrEndpoint ep, BrTcpClient client)
    {
        if (!_clients.TryGetValue(ep.InstanceName, out var current) || !ReferenceEquals(current, client))
            return;

        try
        {
            var network = await provisioning.RegisterConnectionAsync(client, _stopping);
            if (network is null)
            {
                logger.LogWarning("BR {Instance} failed handshake — will retry on reconnect.", ep.InstanceName);
                return;
            }

            _active[ep.InstanceName] = new ActiveBr
            {
                Endpoint = ep,
                Client = client,
                CommandClient = new BrCommandClient(client, _options.RequestTimeout),
                NetworkId = network.Id,
                Status = network.Status,
            };

            logger.LogInformation("BR {Eui64} registered (instance={Instance}, status={Status}).",
                network.Eui64, ep.InstanceName, network.Status);
            await PushNetworkListAsync(CancellationToken.None);
        }
        catch (Exception ex)
        {
            logger.LogWarning(ex, "BR registration failed for {Instance}.", ep.InstanceName);
        }
    }

    private async Task OnClientDisconnectedAsync(BrEndpoint ep)
    {
        if (!_active.TryRemove(ep.InstanceName, out var br))
            return;

        logger.LogInformation("BR {Instance} disconnected.", ep.InstanceName);
        if (br.Status == NetworkStatus.Connected)
            await SafePushAsync(ct => PushConnectionAsync(false, br, ct), CancellationToken.None);
        await PushNetworkListAsync(CancellationToken.None);
    }

    private void OnFrameReceived(BrEndpoint ep, BrTcpClient client, BrFrame frame)
    {
        if (frame.Command != (byte)BrCommand.Notify)
            return;
        if (!_active.TryGetValue(ep.InstanceName, out var br) || br.Status != NetworkStatus.Connected)
            return;

        _ = HandleNotifyAsync(br, frame);
    }

    private async Task HandleNotifyAsync(ActiveBr br, BrFrame frame)
    {
        await _notifyGate.WaitAsync(_stopping);
        try
        {
            var mask = BrNotifyParser.Parse(frame.Payload);
            logger.LogInformation("BR NOTIFY mask=0x{X8}", (uint)mask);

            if (mask.HasFlag(BrChangedMask.Role) || mask.HasFlag(BrChangedMask.Ip))
                await SafePushAsync(ct => PushStateAsync(br, ct), _stopping);
            if (mask.HasFlag(BrChangedMask.Dataset))
                await SafePushAsync(ct => PushDatasetAsync(br, ct), _stopping);
            if (mask.HasFlag(BrChangedMask.RouterTable))
                await SafePushAsync(ct => PushRouterTableAsync(br, ct), _stopping);
            if (mask.HasFlag(BrChangedMask.ChildTable))
                await SafePushAsync(ct => PushChildTableAsync(br, ct), _stopping);
            if (mask.HasFlag(BrChangedMask.JoinerTable))
                await SafePushAsync(ct => PushJoinerTableAsync(br, ct), _stopping);
        }
        catch (OperationCanceledException) when (_stopping.IsCancellationRequested)
        {
        }
        catch (Exception ex)
        {
            logger.LogWarning(ex, "BR NOTIFY handling failed.");
        }
        finally
        {
            _notifyGate.Release();
        }
    }

    private async Task PollActiveAsync(CancellationToken ct)
    {
        foreach (var br in _active.Values.ToArray())
        {
            if (!br.Client.IsConnected)
                continue;

            var network = await provisioning.GetNetworkAsync(br.NetworkId, ct);
            var status = network?.Status ?? br.Status;
            var becameConnected = status == NetworkStatus.Connected && br.Status != NetworkStatus.Connected;
            br.Status = status;

            // Lightweight keepalive for every connection: the firmware state
            // watchdog restarts the BR if no CMD_STATE arrives within 5×15s.
            try
            {
                await br.CommandClient.GetStateAsync(ct);
            }
            catch (OperationCanceledException) when (ct.IsCancellationRequested)
            {
                return;
            }
            catch (Exception ex)
            {
                LogPollFailure(br, ex, "STATE");
            }

            if (status != NetworkStatus.Connected)
                continue;

            if (becameConnected)
            {
                await SafePushAsync(ct2 => PushConnectionAsync(true, br, ct2), ct);
                await PushConnectedSnapshotAsync(br, ct);
                continue;
            }

            await PollHealthAsync(br, ct);
        }
    }

    private async Task PollHealthAsync(ActiveBr br, CancellationToken ct)
    {
        if (Interlocked.Increment(ref _pollCount) % HealthEveryPolls != 0)
            return;

        try
        {
            var health = BrHealthParser.Parse(await br.CommandClient.GetBrHealthAsync(ct));
            await hub.Clients.All.SendAsync(WeaveSignalREvents.BrHealth, BrDtoMapper.ToHealth(health), ct);
        }
        catch (OperationCanceledException) when (ct.IsCancellationRequested)
        {
        }
        catch (Exception ex)
        {
            LogPollFailure(br, ex, "HEALTH");
        }
    }

    private void LogPollFailure(ActiveBr br, Exception ex, string source)
    {
        if (br.Client.IsConnected)
            logger.LogWarning(ex, "BR {Source} poll failed.", source);
        else
            logger.LogDebug(ex, "BR {Source} poll skipped (not connected).", source);
    }

    private async Task PushConnectedSnapshotAsync(ActiveBr br, CancellationToken ct)
    {
        await SafePushAsync(ct2 => PushStateAsync(br, ct2), ct);
        await SafePushAsync(ct2 => PushDatasetAsync(br, ct2), ct);
        await SafePushAsync(ct2 => PushRouterTableAsync(br, ct2), ct);
        await SafePushAsync(ct2 => PushChildTableAsync(br, ct2), ct);
        await SafePushAsync(ct2 => PushJoinerTableAsync(br, ct2), ct);
    }

    private async Task PushConnectionAsync(bool connected, ActiveBr br, CancellationToken ct)
    {
        var dto = connected
            ? new BrConnectionDto(true, br.Endpoint.Host, br.Endpoint.Port)
            : new BrConnectionDto(false, null, null);
        await hub.Clients.All.SendAsync(WeaveSignalREvents.BrConnection, dto, ct);
    }

    private async Task PushStateAsync(ActiveBr br, CancellationToken ct)
    {
        var role = BrStateParser.Parse(await br.CommandClient.GetStateAsync(ct));
        var ip = await TryGetIpAsync(br, ct);
        var version = await TryGetThreadVersionAsync(br, ct);

        br.PublishedRole = role;
        await hub.Clients.All.SendAsync(
            WeaveSignalREvents.BrState,
            BrDtoMapper.ToState(role, ip, version, new BrConnectionDto(true, br.Endpoint.Host, br.Endpoint.Port)),
            ct);
    }

    private async Task PushDatasetAsync(ActiveBr br, CancellationToken ct)
    {
        var payload = await br.CommandClient.GetDatasetActiveAsync(ct);
        var dto = BrDtoMapper.ToDataset(new BrActiveDataset(payload));
        await hub.Clients.All.SendAsync(WeaveSignalREvents.BrDataset, dto, ct);
    }

    private async Task PushRouterTableAsync(ActiveBr br, CancellationToken ct)
    {
        var payload = await br.CommandClient.GetRouterTableAsync(ct);
        var dto = BrTableParser.ParseRouterTable(payload).Select(BrDtoMapper.ToRouterEntry).ToArray();
        await hub.Clients.All.SendAsync(WeaveSignalREvents.BrRouterTable, dto, ct);
    }

    private async Task PushChildTableAsync(ActiveBr br, CancellationToken ct)
    {
        var payload = await br.CommandClient.GetChildTableAsync(ct);
        var dto = BrTableParser.ParseChildTable(payload).Select(BrDtoMapper.ToChildEntry).ToArray();
        await hub.Clients.All.SendAsync(WeaveSignalREvents.BrChildTable, dto, ct);
    }

    private async Task PushJoinerTableAsync(ActiveBr br, CancellationToken ct)
    {
        var payload = await br.CommandClient.GetJoinerTableAsync(ct);
        var dto = BrTableParser.ParseJoinerTable(payload).Select(BrDtoMapper.ToJoinerEntry).ToArray();
        await hub.Clients.All.SendAsync(WeaveSignalREvents.BrJoinerTable, dto, ct);
    }

    private async Task PushNetworkListAsync(CancellationToken ct)
    {
        var networks = await provisioning.ListNetworksAsync(ct);
        var dto = networks.Select(BrDtoMapper.ToNetwork).ToArray();
        await hub.Clients.All.SendAsync(WeaveSignalREvents.NetworkList, dto, ct);
    }

    private async Task SafePushAsync(Func<CancellationToken, Task> push, CancellationToken ct)
    {
        try
        {
            await push(ct);
        }
        catch (OperationCanceledException) when (ct.IsCancellationRequested)
        {
        }
        catch (Exception ex)
        {
            logger.LogDebug(ex, "BR push skipped.");
        }
    }

    private async Task<string?> TryGetIpAsync(ActiveBr br, CancellationToken ct)
    {
        try
        {
            var payload = await br.CommandClient.GetIpAddrAsync(ct);
            return payload.Length >= 16 ? new IPAddress(payload.AsSpan(0, 16)).ToString() : null;
        }
        catch (OperationCanceledException) when (ct.IsCancellationRequested)
        {
            throw;
        }
        catch (Exception ex)
        {
            logger.LogDebug(ex, "BR IP_ADDR query failed.");
            return null;
        }
    }

    private async Task<string?> TryGetThreadVersionAsync(ActiveBr br, CancellationToken ct)
    {
        try
        {
            return Encoding.UTF8.GetString(await br.CommandClient.GetThreadVersionAsync(ct));
        }
        catch (OperationCanceledException) when (ct.IsCancellationRequested)
        {
            throw;
        }
        catch (Exception ex)
        {
            logger.LogDebug(ex, "BR THREAD_VERSION query failed.");
            return null;
        }
    }
}
