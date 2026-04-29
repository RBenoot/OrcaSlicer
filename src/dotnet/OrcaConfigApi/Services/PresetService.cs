using System.Text.Json;
using Microsoft.EntityFrameworkCore;
using OrcaConfigApi.Data;
using OrcaConfigApi.DTOs;
using OrcaConfigApi.Models;

namespace OrcaConfigApi.Services;

public interface IPresetService
{
    Task<List<PresetDto>> GetPresetsAsync(string? type, string? vendorName);
    Task<PresetDto?> GetPresetByIdAsync(Guid id);
    Task<PresetDto> CreatePresetAsync(PresetCreateRequest request, string userId);
    Task<PresetDto?> UpdatePresetAsync(Guid id, PresetUpdateRequest request);
    Task<bool> DeletePresetAsync(Guid id);
}

public class PresetService : IPresetService
{
    private readonly OrcaDbContext _db;

    public PresetService(OrcaDbContext db)
    {
        _db = db;
    }

    public async Task<List<PresetDto>> GetPresetsAsync(string? type, string? vendorName)
    {
        var query = _db.Presets.Include(p => p.Vendor).AsQueryable();

        if (!string.IsNullOrEmpty(type))
            query = query.Where(p => p.Type == type);

        if (!string.IsNullOrEmpty(vendorName))
            query = query.Where(p => p.Vendor != null && p.Vendor.Name == vendorName);

        var presets = await query.OrderBy(p => p.Name).ToListAsync();
        return presets.Select(MapToDto).ToList();
    }

    public async Task<PresetDto?> GetPresetByIdAsync(Guid id)
    {
        var preset = await _db.Presets.Include(p => p.Vendor).FirstOrDefaultAsync(p => p.Id == id);
        return preset == null ? null : MapToDto(preset);
    }

    public async Task<PresetDto> CreatePresetAsync(PresetCreateRequest request, string userId)
    {
        var preset = new Preset
        {
            Id = Guid.NewGuid(),
            Type = request.Type,
            Name = request.Name,
            Inherits = request.Inherits,
            Config = request.Config.RootElement.GetRawText(),
            IsSystem = request.IsSystem,
            IsTemplate = request.IsTemplate,
            FilamentId = request.FilamentId,
            SettingId = request.SettingId ?? Guid.NewGuid().ToString(),
            BaseId = request.BaseId,
            UserId = request.UserId ?? userId,
            SyncStatus = "create",
            UpdatedTime = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
            CreatedAt = DateTime.UtcNow,
            UpdatedAt = DateTime.UtcNow
        };

        _db.Presets.Add(preset);
        await _db.SaveChangesAsync();

        return MapToDto(preset);
    }

    public async Task<PresetDto?> UpdatePresetAsync(Guid id, PresetUpdateRequest request)
    {
        var preset = await _db.Presets.Include(p => p.Vendor).FirstOrDefaultAsync(p => p.Id == id);
        if (preset == null) return null;

        if (request.Name != null) preset.Name = request.Name;
        if (request.Inherits != null) preset.Inherits = request.Inherits;
        if (request.Config != null) preset.Config = request.Config.RootElement.GetRawText();
        if (request.SyncStatus != null) preset.SyncStatus = request.SyncStatus;
        if (request.UpdatedTime.HasValue) preset.UpdatedTime = request.UpdatedTime.Value;

        preset.UpdatedAt = DateTime.UtcNow;
        await _db.SaveChangesAsync();

        return MapToDto(preset);
    }

    public async Task<bool> DeletePresetAsync(Guid id)
    {
        var preset = await _db.Presets.FindAsync(id);
        if (preset == null) return false;

        preset.SyncStatus = "delete";
        preset.UpdatedAt = DateTime.UtcNow;
        await _db.SaveChangesAsync();
        return true;
    }

    private static PresetDto MapToDto(Preset preset)
    {
        return new PresetDto(
            preset.Id,
            preset.VendorId,
            preset.Vendor?.Name,
            preset.Type,
            preset.Name,
            preset.Inherits,
            JsonDocument.Parse(preset.Config),
            preset.IsSystem,
            preset.IsTemplate,
            preset.FilamentId,
            preset.SettingId,
            preset.BaseId,
            preset.SyncStatus,
            preset.UpdatedTime,
            preset.UserId,
            ((DateTimeOffset)preset.CreatedAt).ToUnixTimeMilliseconds(),
            ((DateTimeOffset)preset.UpdatedAt).ToUnixTimeMilliseconds()
        );
    }
}
