using Microsoft.EntityFrameworkCore;
using OrcaConfigApi.Models;

namespace OrcaConfigApi.Data;

public class OrcaDbContext : DbContext
{
    public OrcaDbContext(DbContextOptions<OrcaDbContext> options) : base(options) { }

    public DbSet<Vendor> Vendors => Set<Vendor>();
    public DbSet<Preset> Presets => Set<Preset>();
    public DbSet<User> Users => Set<User>();
    public DbSet<PhysicalPrinter> PhysicalPrinters => Set<PhysicalPrinter>();
    public DbSet<UserSelection> UserSelections => Set<UserSelection>();
    public DbSet<SyncState> SyncStates => Set<SyncState>();

    protected override void OnModelCreating(ModelBuilder modelBuilder)
    {
        modelBuilder.Entity<Vendor>(entity =>
        {
            entity.HasKey(e => e.Id);
            entity.HasIndex(e => e.Name).IsUnique();
        });

        modelBuilder.Entity<Preset>(entity =>
        {
            entity.HasKey(e => e.Id);
            entity.HasIndex(e => new { e.VendorId, e.Type, e.Name }).IsUnique();
            entity.Property(e => e.Config).HasColumnType("jsonb");
            entity.HasIndex(e => e.Type);
            entity.HasIndex(e => e.SettingId);
            entity.HasIndex(e => e.FilamentId);
            entity.HasIndex(e => e.Inherits);
            entity.HasIndex(e => e.SyncStatus);
            
            entity.HasOne(p => p.Vendor)
                .WithMany(v => v.Presets)
                .HasForeignKey(p => p.VendorId)
                .OnDelete(DeleteBehavior.SetNull);
        });

        modelBuilder.Entity<User>(entity =>
        {
            entity.HasKey(e => e.Id);
            entity.HasIndex(e => e.Username).IsUnique();
        });

        modelBuilder.Entity<PhysicalPrinter>(entity =>
        {
            entity.HasKey(e => e.Id);
            entity.HasIndex(e => e.Name).IsUnique();
        });

        modelBuilder.Entity<UserSelection>(entity =>
        {
            entity.HasKey(e => e.Id);
            entity.HasIndex(e => new { e.UserId, e.PrinterName }).IsUnique();
        });

        modelBuilder.Entity<SyncState>(entity =>
        {
            entity.HasKey(e => e.UserId);
        });
    }
}
