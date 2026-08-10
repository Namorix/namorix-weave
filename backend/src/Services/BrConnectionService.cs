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

namespace Namorix.Weave.Services;

public sealed class BrConnectionService(
    BrTcpClient transport,
    BrCommandClient client,
    IHubContext<BrHub> hub,
    IOptions<BorderRouterOptions> options,
    ILogger<BrConnectionService> logger)
    : BackgroundService
{
    private const int HealthEveryPolls = 3;

    private readonly BorderRouterOptions _options = options.Value;

    private readonly SemaphoreSlim _notifyGate = new(1, 1);

    private CancellationTokenSource? _stopping;
    private BrRole? _publishedRole;

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        _stopping = CancellationTokenSource.CreateLinkedTokenSource(stoppingToken);
        var token = _stopping.Token;

        transport.Connected += OnConnected;
        transport.Disconnected += OnDisconnected;
        transport.FrameReceived += OnFrameReceived;

        transport.Start(token);

        using var timer = new PeriodicTimer(TimeSpan.FromSeconds(_options.StatePollIntervalSec));
        var pollCount = 0;

        try
        {
            while (!token.IsCancellationRequested)
            {
                await timer.WaitForNextTickAsync(token);
                pollCount++;
                await PollStateAsync(token);
                if (pollCount % HealthEveryPolls == 0)
                    await PollHealthAsync(token);
            }
        }
        catch (OperationCanceledException) when (token.IsCancellationRequested)
        {
        }
        finally
        {
            transport.Connected -= OnConnected;
            transport.Disconnected -= OnDisconnected;
            transport.FrameReceived -= OnFrameReceived;
            await _stopping.CancelAsync();
        }
    }

    /* ---- transport events (run on the TCP read-loop thread) ---- */

    private void OnConnected() => _ = HandleConnectedAsync();

    private void OnDisconnected() => _ = HandleDisconnectedAsync();

    private void OnFrameReceived(BrFrame frame)
    {
        if (frame.Command == (byte)BrCommand.Notify)
            _ = HandleNotifyAsync(frame);
    }

    private async Task HandleConnectedAsync()
    {
        var token = _stopping?.Token ?? CancellationToken.None;
        logger.LogInformation("BR connected — pushing initial snapshot.");
        await SafePushAsync(ct => PushConnectionAsync(true, ct), token);
        await SafePushAsync(PushStateAsync, token);
        await SafePushAsync(PushDatasetAsync, token);
        await SafePushAsync(PushRouterTableAsync, token);
        await SafePushAsync(PushChildTableAsync, token);
        await SafePushAsync(PushJoinerTableAsync, token);
    }

    private async Task HandleDisconnectedAsync()
    {
        var token = _stopping?.Token ?? CancellationToken.None;
        _publishedRole = null;
        await SafePushAsync(ct => PushConnectionAsync(false, ct), token);
    }

    private async Task HandleNotifyAsync(BrFrame frame)
    {
        await _notifyGate.WaitAsync(_stopping?.Token ?? CancellationToken.None);
        try
        {
            var mask = BrNotifyParser.Parse(frame.Payload);
            logger.LogInformation("BR NOTIFY mask=0x{X8}", (uint)mask);

            if (mask.HasFlag(BrChangedMask.Role) || mask.HasFlag(BrChangedMask.Ip))
                await SafePushAsync(PushStateAsync, _stopping?.Token ?? CancellationToken.None);
            if (mask.HasFlag(BrChangedMask.Dataset))
                await SafePushAsync(PushDatasetAsync, _stopping?.Token ?? CancellationToken.None);
            if (mask.HasFlag(BrChangedMask.RouterTable))
                await SafePushAsync(PushRouterTableAsync, _stopping?.Token ?? CancellationToken.None);
            if (mask.HasFlag(BrChangedMask.ChildTable))
                await SafePushAsync(PushChildTableAsync, _stopping?.Token ?? CancellationToken.None);
            if (mask.HasFlag(BrChangedMask.JoinerTable))
                await SafePushAsync(PushJoinerTableAsync, _stopping?.Token ?? CancellationToken.None);
        }
        catch (OperationCanceledException) when (_stopping?.IsCancellationRequested == true)
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

    /* ---- polling ---- */

    private async Task PollStateAsync(CancellationToken ct)
    {
        try
        {
            var role = BrStateParser.Parse(await client.GetStateAsync(ct));
            if (role != _publishedRole)
                await PushStateAsync(ct);
        }
        catch (OperationCanceledException) when (ct.IsCancellationRequested)
        {
        }
        catch (Exception ex)
        {
            LogPollFailure(ex, "STATE");
        }
    }

    private async Task PollHealthAsync(CancellationToken ct)
    {
        try
        {
            var health = BrHealthParser.Parse(await client.GetBrHealthAsync(ct));
            await hub.Clients.All.SendAsync(WeaveSignalREvents.BrHealth, BrDtoMapper.ToHealth(health), ct);
        }
        catch (OperationCanceledException) when (ct.IsCancellationRequested)
        {
        }
        catch (Exception ex)
        {
            LogPollFailure(ex, "HEALTH");
        }
    }

    private void LogPollFailure(Exception ex, string source)
    {
        // While fully disconnected the transport already logs the reconnect loop;
        // only surface poll errors once a BR connection was actually established.
        if (transport.IsConnected)
            logger.LogWarning(ex, "BR {Source} poll failed.", source);
        else
            logger.LogDebug(ex, "BR {Source} poll skipped (not connected).", source);
    }

    /* ---- SignalR pushes ---- */

    private async Task PushConnectionAsync(bool connected, CancellationToken ct)
    {
        await hub.Clients.All.SendAsync(WeaveSignalREvents.BrConnection, CurrentConnection(), ct);
    }

    private async Task PushStateAsync(CancellationToken ct)
    {
        var role = BrStateParser.Parse(await client.GetStateAsync(ct));
        var ip = await TryGetIpAsync(ct);
        var version = await TryGetThreadVersionAsync(ct);

        _publishedRole = role;
        await hub.Clients.All.SendAsync(WeaveSignalREvents.BrState, BrDtoMapper.ToState(role, ip, version, CurrentConnection()), ct);
    }

    private async Task PushDatasetAsync(CancellationToken ct)
    {
        var payload = await client.GetDatasetActiveAsync(ct);
        var dto = BrDtoMapper.ToDataset(new BrActiveDataset(payload));
        await hub.Clients.All.SendAsync(WeaveSignalREvents.BrDataset, dto, ct);
    }

    private async Task PushRouterTableAsync(CancellationToken ct)
    {
        var payload = await client.GetRouterTableAsync(ct);
        var dto = BrTableParser.ParseRouterTable(payload).Select(BrDtoMapper.ToRouterEntry).ToArray();
        await hub.Clients.All.SendAsync(WeaveSignalREvents.BrRouterTable, dto, ct);
    }

    private async Task PushChildTableAsync(CancellationToken ct)
    {
        var payload = await client.GetChildTableAsync(ct);
        var dto = BrTableParser.ParseChildTable(payload).Select(BrDtoMapper.ToChildEntry).ToArray();
        await hub.Clients.All.SendAsync(WeaveSignalREvents.BrChildTable, dto, ct);
    }

    private async Task PushJoinerTableAsync(CancellationToken ct)
    {
        var payload = await client.GetJoinerTableAsync(ct);
        var dto = BrTableParser.ParseJoinerTable(payload).Select(BrDtoMapper.ToJoinerEntry).ToArray();
        await hub.Clients.All.SendAsync(WeaveSignalREvents.BrJoinerTable, dto, ct);
    }

    /* ---- helpers ---- */

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

    private async Task<string?> TryGetIpAsync(CancellationToken ct)
    {
        try
        {
            var payload = await client.GetIpAddrAsync(ct);
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

    private async Task<string?> TryGetThreadVersionAsync(CancellationToken ct)
    {
        try
        {
            return Encoding.UTF8.GetString(await client.GetThreadVersionAsync(ct));
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

    private BrConnectionDto CurrentConnection() =>
        new(transport.IsConnected, _options.Host, _options.Port);
}
