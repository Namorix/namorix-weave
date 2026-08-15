using Microsoft.AspNetCore.Mvc;
using Namorix.Core.Middleware;
using Namorix.Core.Responses;
using Namorix.Weave.Dtos;
using Namorix.Weave.Services.BorderRouter;

namespace Namorix.Weave.Controllers;

[ApiController]
[RequireAuth]
[Route("api/networks")]
public sealed class NetworkController(BrConnectionService connections) : ControllerBase
{
    [HttpPost("{id:int}/accept")]
    public async Task<IActionResult> Accept(
        int id, [FromBody] NetworkAcceptRequest request, CancellationToken ct)
    {
        NetworkDto? network;
        try
        {
            network = await connections.AcceptAsync(id, request, ct);
        }
        catch (ArgumentException ex)
        {
            return BadRequest(ApiResponse.Fail("INVALID_DATASET", ex.Message));
        }

        return network is null
            ? NotFound(ApiResponse.Fail("NETWORK_NOT_FOUND", "Network not found or not in Pending state."))
            : Ok(ApiResponse.Ok(network));
    }

    [HttpPost("{id:int}/reject")]
    public async Task<IActionResult> Reject(int id, CancellationToken ct)
    {
        var network = await connections.RejectAsync(id, ct);

        return network is null
            ? NotFound(ApiResponse.Fail("NETWORK_NOT_FOUND", "Network not found or not in Pending state."))
            : Ok(ApiResponse.Ok(network));
    }
}
