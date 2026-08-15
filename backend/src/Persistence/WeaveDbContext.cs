using Microsoft.EntityFrameworkCore;
using Namorix.Weave.Models;

namespace Namorix.Weave.Persistence;

public sealed class WeaveDbContext(DbContextOptions<WeaveDbContext> options) : DbContext(options)
{
    public DbSet<Network> Networks => Set<Network>();

    public DbSet<BrThreadDataset> BrThreadDataset => Set<BrThreadDataset>();

    public DbSet<AddonSession> Sessions => Set<AddonSession>();

    protected override void OnModelCreating(ModelBuilder modelBuilder)
    {
        modelBuilder.Entity<Network>(entity =>
        {
            entity.Property(n => n.Protocol).HasConversion<string>();
            entity.Property(n => n.Status).HasConversion<string>();

            entity.HasIndex(n => n.Eui64)
                .IsUnique()
                .HasFilter("\"Eui64\" IS NOT NULL");

            entity.HasOne(n => n.ThreadDataset)
                .WithOne(d => d.Network)
                .HasForeignKey<BrThreadDataset>(d => d.NetworkId)
                .OnDelete(DeleteBehavior.Cascade);
        });

        modelBuilder.Entity<BrThreadDataset>(entity =>
        {
            entity.HasKey(d => d.NetworkId);
        });
    }
}
