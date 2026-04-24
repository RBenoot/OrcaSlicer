namespace OrcaConfigApi.Models;

public class UserSelection
{
    public Guid Id { get; set; }
    public string UserId { get; set; } = string.Empty;
    public string? PrinterName { get; set; }
    public Guid? PrintPresetId { get; set; }
    public List<Guid> FilamentPresetIds { get; set; } = new();
    public string Config { get; set; } = "{}";
    public DateTime UpdatedAt { get; set; }
    
    public Preset? PrintPreset { get; set; }
}
