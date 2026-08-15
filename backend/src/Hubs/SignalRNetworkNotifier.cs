using Microsoft.AspNetCore.SignalR;
using Namorix.Weave.Constants;
using Namorix.Weave.Dtos;
using Namorix.Weave.Infrastructure;

namespace Namorix.Weave.Hubs;

public class SignalRNetworkNotifier(IHubContext<WeaveHub> hubContext) : INetworkNotifier
{
    public async Task NotifyListChanged()
    {
        await hubContext.Clients.Group(WeaveSignalRGroups.Network)
            .SendAsync(WeaveSignalREvents.NetworkListChanged, new { });
    }

    public async Task NotifyChanged(NetworkDto network)
    {
        await hubContext.Clients.Group(WeaveSignalRGroups.Network)
            .SendAsync(WeaveSignalREvents.NetworkChanged, network);
    }
}
