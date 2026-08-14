using Microsoft.EntityFrameworkCore;
using Microsoft.EntityFrameworkCore.Design;

namespace Namorix.Weave.Persistence;

public sealed class WeaveDbContextFactory : IDesignTimeDbContextFactory<WeaveDbContext>
{
    public WeaveDbContext CreateDbContext(string[] args)
    {
        var dataDir = Environment.GetEnvironmentVariable("NMX_DATA_DIR") ?? "./data";
        var connection = Environment.GetEnvironmentVariable("WEAVE_DB_CONNECTION")
                         ?? $"Data Source={Path.Combine(dataDir, "weave.db")}";
        var options = new DbContextOptionsBuilder<WeaveDbContext>()
            .UseSqlite(connection)
            .Options;
        return new WeaveDbContext(options);
    }
}