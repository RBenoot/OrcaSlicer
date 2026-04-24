namespace OrcaConfigApi.Models;

public class PhysicalPrinter
{
    public Guid Id { get; set; }
    public string Name { get; set; } = string.Empty;
    public string? PrinterModel { get; set; }
    public Guid? PresetId { get; set; }
    public string Config { get; set; } = "{}";
    public DateTime CreatedAt { get; set; }
    public DateTime UpdatedAt { get; set; }
    
    public Preset? Preset { get; set; }
}
