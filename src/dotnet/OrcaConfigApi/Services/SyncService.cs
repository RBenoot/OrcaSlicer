using System.Text.Json;
using Microsoft.EntityFrameworkCore;
using OrcaConfigApi.Data;
using OrcaConfigApi.DTOs;
using OrcaConfigApi.Models;

namespace OrcaConfigApi.Services;

public interface ISyncService
{
    Task<SyncPullResponse> PullAsync(string? cursor, int limit = 100);
    Task<SyncPushResponse> PushAsync(SyncPushRequest request, string userId);
    Task<SyncStateDto?> GetStateAsync(string userId);
}

public class SyncService : ISyncService
{
    private readonly OrcaDbContext _db;

    public SyncService(OrcaDbContext db)
    {
        _db = db;
    }

    public async Task<SyncPullResponse> PullAsync(string? cursor, int limit = 100)
    {
        var query = _db.Presets.Include(p => p.Vendor).AsQueryable();

        if (!string.IsNullOrEmpty(cursor))
        {
            var cursorParts = cursor.Split(':');
            if (cursorParts.Length == 2 && long.TryParse(cursorParts[1], out var timestamp))
            {
                query = query.Where(p => p.UpdatedAt.Ticks > timestamp);
            }
        }

        var presets = await query
            .Where(p => p.SyncStatus != "delete")
            .OrderBy(p => p.UpdatedAt)
            .Take(limit + 1)
            .ToListAsync();

        var hasMore = presets.Count > limit;
        if (hasMore) presets = presets.Take(limit).ToList();

        var nextCursor = hasMore && presets.Count > 0
            ? $"{presets.Last().Id}:{presets.Last().UpdatedAt.Ticks}"
            : null;

        var deletes = await _db.Presets
            .Where(p => p.SyncStatus == "delete")
            .Select(p => p.SettingId ?? p.Id.ToString())
            .ToListAsync();

        return new SyncPullResponse(
            nextCursor,
            presets.Select(MapToDto).ToList(),
            deletes
        );
    }

    public async Task<SyncPushResponse> PushAsync(SyncPushRequest request, string userId)
    {
        var results = new List<SyncPushResult>();

        foreach (var change in request.Changes)
        {
            try
            {
                var result = await ProcessChangeAsync(change, userId);
                results.Add(result);
            }
            catch (Exception ex)
            {
                results.Add(new SyncPushResult(false, change.SettingId, null, null, ex.Message));
            }
        }

        return new SyncPushResponse(results);
    }

    public async Task<SyncStateDto?> GetStateAsync(string userId)
    {
        var state = await _db.SyncStates.FindAsync(userId);
        if (state == null) return null;

        return new SyncStateDto(state.UserId, state.LastSyncTimestamp, state.SyncCursor);
    }

    private async Task<SyncPushResult> ProcessChangeAsync(SyncChange change, string userId)
    {
        Preset? existing = null;

        if (change.Id.HasValue)
            existing = await _db.Presets.FindAsync(change.Id.Value);
        else if (!string.IsNullOrEmpty(change.SettingId))
            existing = await _db.Presets.FirstOrDefaultAsync(p => p.SettingId == change.SettingId);

        if (change.SyncStatus == "delete")
        {
            if (existing != null)
            {
                existing.SyncStatus = "delete";
                existing.UpdatedAt = DateTime.UtcNow;
            }
            await _db.SaveChangesAsync();
            return new SyncPushResult(true, change.SettingId, existing?.Id, null, null);
        }

        if (existing == null)
        {
            var preset = new Preset
            {
                Id = change.Id ?? Guid.NewGuid(),
                Type = change.Type,
                Name = change.Name,
                Config = change.Content.RootElement.GetRawText(),
                SettingId = change.SettingId ?? Guid.NewGuid().ToString(),
                SyncStatus = "save",
                UpdatedTime = change.UpdatedAt,
                UserId = userId,
                CreatedAt = DateTime.UtcNow,
                UpdatedAt = DateTime.UtcNow
            };
            _db.Presets.Add(preset);
            await _db.SaveChangesAsync();
            return new SyncPushResult(true, preset.SettingId, preset.Id, preset.UpdatedTime, null);
        }

        if (change.UpdatedAt >= existing.UpdatedTime)
        {
            existing.Name = change.Name;
            existing.Config = change.Content.RootElement.GetRawText();
            existing.SyncStatus = "save";
            existing.UpdatedTime = change.UpdatedAt;
            existing.UpdatedAt = DateTime.UtcNow;
            await _db.SaveChangesAsync();
        }

        var syncState = await _db.SyncStates.FindAsync(userId);
        if (syncState == null)
        {
            syncState = new SyncState { UserId = userId };
            _db.SyncStates.Add(syncState);
        }
        syncState.LastSyncTimestamp = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
        syncState.UpdatedAt = DateTime.UtcNow;
        await _db.SaveChangesAsync();

        return new SyncPushResult(true, existing.SettingId, existing.Id, existing.UpdatedTime, null);
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
            preset.CreatedAt,
            preset.UpdatedAt
        );
    }
}
