using Microsoft.AspNetCore.SignalR;
using Namorix.Core.Hubs;
using Namorix.Weave.Constants;

namespace Namorix.Weave.Hubs;

public sealed class WeaveHub(ILogger<NmxHub> logger) : NmxHub(logger)
{
    public async Task SubscribeNetwork()
    {
        await Groups.AddToGroupAsync(Context.ConnectionId, WeaveSignalRGroups.Network);
    }

    public async Task UnsubscribeNetwork()
    {
        await Groups.RemoveFromGroupAsync(Context.ConnectionId, WeaveSignalRGroups.Network);
    }

    public async Task SubscribeBorderRouter()
    {
        await Groups.AddToGroupAsync(Context.ConnectionId, WeaveSignalRGroups.BorderRouter);
    }

    public async Task UnsubscribeBorderRouter()
    {
        await Groups.RemoveFromGroupAsync(Context.ConnectionId, WeaveSignalRGroups.BorderRouter);
    }
}
