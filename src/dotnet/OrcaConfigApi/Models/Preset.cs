namespace OrcaConfigApi.Models;

public class Preset
{
    public Guid Id { get; set; }
    public Guid? VendorId { get; set; }
    
    public string Type { get; set; } = string.Empty;
    public string Name { get; set; } = string.Empty;
    public string? Inherits { get; set; }
    
    public string Config { get; set; } = "{}";
    
    public bool IsSystem { get; set; }
    public bool IsTemplate { get; set; }
    public string? FilamentId { get; set; }
    public string? SettingId { get; set; }
    public string? BaseId { get; set; }
    
    public string SyncStatus { get; set; } = string.Empty;
    public long UpdatedTime { get; set; }
    public string? UserId { get; set; }
    
    public DateTime CreatedAt { get; set; }
    public DateTime UpdatedAt { get; set; }
    
    public Vendor? Vendor { get; set; }
}
