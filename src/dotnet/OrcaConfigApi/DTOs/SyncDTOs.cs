using System.Text.Json;

namespace OrcaConfigApi.DTOs;

public record SyncPullResponse(
    string? NextCursor,
    List<PresetDto> Upserts,
    List<string> Deletes
);

public record SyncPushRequest(
    List<SyncChange> Changes
);

public record SyncChange(
    Guid? Id,
    string? SettingId,
    string Name,
    string Type,
    JsonDocument Content,
    string SyncStatus,
    long UpdatedAt
);

public record SyncPushResult(
    bool Success,
    string? SettingId,
    Guid? NewId,
    long? NewUpdatedAt,
    string? Error
);

public record SyncPushResponse(
    List<SyncPushResult> Results
);

public record SyncStateDto(
    string UserId,
    long LastSyncTimestamp,
    string? SyncCursor
);
