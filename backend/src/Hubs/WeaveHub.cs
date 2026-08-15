using Microsoft.AspNetCore.SignalR;
using Namorix.Core.Hubs;
using Namorix.Weave.BorderRouter.Dtos;
using Namorix.Weave.Dtos;
using Namorix.Weave.Services.BorderRouter;

namespace Namorix.Weave.Hubs;

public sealed class WeaveHub(
    BrConnectionService connections,
    ILogger<NmxHub> logger) : NmxHub(logger)
{
    public async Task<NetworkDto> AcceptNetwork(int networkId, NetworkAcceptRequest request, CancellationToken ct)
    {
        try
        {
            return await connections.AcceptAsync(networkId, request, ct)
                ?? throw new HubException("Network not found or not in Pending state.");
        }
        catch (ArgumentException ex)
        {
            throw new HubException(ex.Message);
        }
    }

    public async Task<NetworkDto> RejectNetwork(int networkId, CancellationToken ct)
    {
        return await connections.RejectAsync(networkId, ct)
            ?? throw new HubException("Network not found or not in Pending state.");
    }
}
