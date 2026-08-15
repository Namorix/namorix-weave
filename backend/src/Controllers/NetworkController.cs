using Microsoft.AspNetCore.Mvc;
using Namorix.Core.Middleware;
using Namorix.Core.Responses;
using Namorix.Weave.BorderRouter.Dtos;
using Namorix.Weave.Constants;
using Namorix.Weave.Dtos;
using Namorix.Weave.Services.BorderRouter;

namespace Namorix.Weave.Controllers;

[ApiController]
[RequireAuth]
[Route("api/networks")]
public sealed class NetworkController(BrConnectionService connections, BrProvisioningService provisioning)
    : ControllerBase
{
    [HttpGet]
    public async Task<IActionResult> List(CancellationToken ct)
    {
        var networks = await provisioning.ListNetworksAsync(ct);
        return Ok(ApiResponse.Ok(networks.Select(BrDtoMapper.ToNetwork).ToArray()));
    }

    [HttpGet("{id:int}")]
    public async Task<IActionResult> Get(int id, CancellationToken ct)
    {
        var network = await provisioning.GetNetworkAsync(id, ct);
        return network is null
            ? NotFound(ApiResponse.Fail(Error.NetworkNotFound, "Network not found."))
            : Ok(ApiResponse.Ok(BrDtoMapper.ToNetwork(network)));
    }

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
            return BadRequest(ApiResponse.Fail(Error.InvalidDataset, ex.Message));
        }

        return network is null
            ? NotFound(ApiResponse.Fail(Error.NetworkNotFound, "Network not found or not in Pending state."))
            : Ok(ApiResponse.Ok(network));
    }

    [HttpPost("{id:int}/reject")]
    public async Task<IActionResult> Reject(int id, CancellationToken ct)
    {
        var network = await connections.RejectAsync(id, ct);

        return network is null
            ? NotFound(ApiResponse.Fail(Error.NetworkNotFound, "Network not found or not in Pending state."))
            : Ok(ApiResponse.Ok(network));
    }

    // BR/Thread-specific live-data snapshot (prefix `br` — Zigbee dùng prefix riêng sau này).
    [HttpGet("{id:int}/br/state")]
    public Task<IActionResult> GetBrState(int id, CancellationToken ct) =>
        WithBrSnapshot(id, c => connections.GetStateAsync(id, c), ct);

    [HttpGet("{id:int}/br/dataset")]
    public Task<IActionResult> GetBrDataset(int id, CancellationToken ct) =>
        WithBrSnapshot(id, c => connections.GetDatasetAsync(id, c), ct);

    [HttpGet("{id:int}/br/tables")]
    public Task<IActionResult> GetBrTables(int id, CancellationToken ct) =>
        WithBrSnapshot(id, c => connections.GetTablesAsync(id, c), ct);

    private async Task<IActionResult> WithBrSnapshot<T>(
        int id, Func<CancellationToken, Task<T?>> fetch, CancellationToken ct)
    {
        var network = await provisioning.GetNetworkAsync(id, ct);
        if (network is null)
            return NotFound(ApiResponse.Fail(Error.NetworkNotFound, "Network not found."));

        var dto = await fetch(ct);
        return dto is null
            ? Conflict(ApiResponse.Fail(Error.BrNotConnected, "Border router is not connected."))
            : Ok(ApiResponse.Ok(dto));
    }
}
