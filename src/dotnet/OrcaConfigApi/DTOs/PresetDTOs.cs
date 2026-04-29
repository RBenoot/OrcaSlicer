using System.Text.Json;

namespace OrcaConfigApi.DTOs;

public record PresetDto(
    Guid Id,
    Guid? VendorId,
    string? VendorName,
    string Type,
    string Name,
    string? Inherits,
    JsonDocument? Config,
    bool IsSystem,
    bool IsTemplate,
    string? FilamentId,
    string? SettingId,
    string? BaseId,
    string SyncStatus,
    long UpdatedTime,
    string? UserId,
    long CreatedAt,
    long UpdatedAt
);

public record PresetCreateRequest(
    string Type,
    string Name,
    string? Inherits,
    JsonDocument Config,
    bool IsSystem,
    bool IsTemplate,
    string? FilamentId,
    string? SettingId,
    string? BaseId,
    string? UserId
);

public record PresetUpdateRequest(
    string? Name,
    string? Inherits,
    JsonDocument? Config,
    string? SyncStatus,
    long? UpdatedTime
);
