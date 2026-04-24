using System.Security.Claims;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using OrcaConfigApi.DTOs;
using OrcaConfigApi.Services;

namespace OrcaConfigApi.Controllers;

[ApiController]
[Route("api/[controller]")]
[Authorize]
public class SyncController : ControllerBase
{
    private readonly ISyncService _syncService;

    public SyncController(ISyncService syncService)
    {
        _syncService = syncService;
    }

    [HttpGet("pull")]
    public async Task<IActionResult> Pull([FromQuery] string? cursor, [FromQuery] int limit = 100)
    {
        var result = await _syncService.PullAsync(cursor, limit);
        return Ok(result);
    }

    [HttpPost("push")]
    public async Task<IActionResult> Push([FromBody] SyncPushRequest request)
    {
        var userId = User.FindFirst(ClaimTypes.NameIdentifier)?.Value ?? "anonymous";
        var result = await _syncService.PushAsync(request, userId);
        return Ok(result);
    }

    [HttpGet("state")]
    public async Task<IActionResult> GetState()
    {
        var userId = User.FindFirst(ClaimTypes.NameIdentifier)?.Value;
        if (string.IsNullOrEmpty(userId))
            return Unauthorized();

        var state = await _syncService.GetStateAsync(userId);
        if (state == null)
            return Ok(new SyncStateDto(userId, 0, null));
        return Ok(state);
    }
}
