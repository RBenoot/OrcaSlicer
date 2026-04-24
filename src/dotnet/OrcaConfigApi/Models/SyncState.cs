namespace OrcaConfigApi.Models;

public class SyncState
{
    public string UserId { get; set; } = string.Empty;
    public long LastSyncTimestamp { get; set; }
    public string? SyncCursor { get; set; }
    public DateTime UpdatedAt { get; set; }
}
