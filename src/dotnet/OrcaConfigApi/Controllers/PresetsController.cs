using System.Security.Claims;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using OrcaConfigApi.DTOs;
using OrcaConfigApi.Services;

namespace OrcaConfigApi.Controllers;

[ApiController]
[Route("api/[controller]")]
[Authorize]
public class PresetsController : ControllerBase
{
    private readonly IPresetService _presetService;

    public PresetsController(IPresetService presetService)
    {
        _presetService = presetService;
    }

    [HttpGet]
    public async Task<IActionResult> GetPresets([FromQuery] string? type, [FromQuery] string? vendor)
    {
        var presets = await _presetService.GetPresetsAsync(type, vendor);
        return Ok(presets);
    }

    [HttpGet("{id:guid}")]
    public async Task<IActionResult> GetPreset(Guid id)
    {
        var preset = await _presetService.GetPresetByIdAsync(id);
        if (preset == null)
            return NotFound();
        return Ok(preset);
    }

    [HttpPost]
    public async Task<IActionResult> CreatePreset([FromBody] PresetCreateRequest request)
    {
        var userId = User.FindFirst(ClaimTypes.NameIdentifier)?.Value ?? "anonymous";
        var preset = await _presetService.CreatePresetAsync(request, userId);
        return CreatedAtAction(nameof(GetPreset), new { id = preset.Id }, preset);
    }

    [HttpPut("{id:guid}")]
    public async Task<IActionResult> UpdatePreset(Guid id, [FromBody] PresetUpdateRequest request)
    {
        var preset = await _presetService.UpdatePresetAsync(id, request);
        if (preset == null)
            return NotFound();
        return Ok(preset);
    }

    [HttpDelete("{id:guid}")]
    public async Task<IActionResult> DeletePreset(Guid id)
    {
        var result = await _presetService.DeletePresetAsync(id);
        if (!result)
            return NotFound();
        return NoContent();
    }
}
