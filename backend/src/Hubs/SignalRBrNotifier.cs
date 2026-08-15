using Microsoft.AspNetCore.SignalR;
using Namorix.Weave.Constants;
using Namorix.Weave.Infrastructure;

namespace Namorix.Weave.Hubs;

public class SignalRBrNotifier(IHubContext<WeaveHub> hubContext) : IBrNotifier
{
    public async Task NotifyConnectionChanged(int networkId, bool isConnected, string? host, int? port)
    {
        await hubContext.Clients.Group(WeaveSignalRGroups.BorderRouter)
            .SendAsync(WeaveSignalREvents.BrConnection, new { networkId, isConnected, host, port });
    }

    public async Task NotifyStateChanged(int networkId)
    {
        await hubContext.Clients.Group(WeaveSignalRGroups.BorderRouter)
            .SendAsync(WeaveSignalREvents.BrState, new { networkId });
    }

    public async Task NotifyHealthChanged(int networkId)
    {
        await hubContext.Clients.Group(WeaveSignalRGroups.BorderRouter)
            .SendAsync(WeaveSignalREvents.BrHealth, new { networkId });
    }

    public async Task NotifyDatasetChanged(int networkId)
    {
        await hubContext.Clients.Group(WeaveSignalRGroups.BorderRouter)
            .SendAsync(WeaveSignalREvents.BrDataset, new { networkId });
    }

    public async Task NotifyRouterTableChanged(int networkId)
    {
        await hubContext.Clients.Group(WeaveSignalRGroups.BorderRouter)
            .SendAsync(WeaveSignalREvents.BrRouterTable, new { networkId });
    }

    public async Task NotifyChildTableChanged(int networkId)
    {
        await hubContext.Clients.Group(WeaveSignalRGroups.BorderRouter)
            .SendAsync(WeaveSignalREvents.BrChildTable, new { networkId });
    }

    public async Task NotifyJoinerTableChanged(int networkId)
    {
        await hubContext.Clients.Group(WeaveSignalRGroups.BorderRouter)
            .SendAsync(WeaveSignalREvents.BrJoinerTable, new { networkId });
    }
}
