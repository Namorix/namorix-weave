using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Options;
using Namorix.Core.Grpc;
using Namorix.Core.IO;
using Namorix.Core.OAuth;
using Namorix.Weave.BorderRouter;
using Namorix.Weave.Constants;
using Namorix.Weave.Hubs;
using Namorix.Weave.Persistence;
using Namorix.Weave.Services;
using Namorix.Weave.Services.BorderRouter;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddNmxOAuth2Client();
builder.Services.AddAddonChannelClient();
builder.Services.AddHostedService<WeaveService>();

builder.Services.AddSignalR();
builder.Services.AddOptions<BrOptions>()
    .Bind(builder.Configuration.GetSection(BrOptions.SectionName))
    .ValidateDataAnnotations()
    .ValidateOnStart();

// Persistence: DB lives alongside oauth.json under the addon DataDir (NMX_DATA_DIR).
// Weave already references Namorix.Core, so DataDirectory gives the resolved base path.
var addon = NmxAddonConfig.FromEnvironment();
builder.Services.AddSingleton(new DataDirectory(addon.DataDir));
builder.Services.AddSingleton(new SecretProtector(addon.DataDir));

var dbPath = Path.Combine(addon.DataDir, "weave.db");
builder.Services.AddDbContextFactory<WeaveDbContext>(options =>
    options.UseSqlite($"Data Source={dbPath}"));

// BR advertises _thread-border-router-frame._tcp via mDNS; backend browses then connects out.
builder.Services.AddSingleton(sp =>
{
    var options = sp.GetRequiredService<IOptions<BrOptions>>().Value;
    return new BrMdnsBrowser(options.MdnsServiceName, options.FramePort, sp.GetRequiredService<ILogger<BrMdnsBrowser>>());
});

builder.Services.AddSingleton(sp =>
{
    var timeout = sp.GetRequiredService<IOptions<BrOptions>>().Value.RequestTimeout;
    return new BrProvisioningService(
        timeout,
        sp.GetRequiredService<IDbContextFactory<WeaveDbContext>>(),
        sp.GetRequiredService<ILogger<BrProvisioningService>>());
});

builder.Services.AddHostedService<BrConnectionService>();

var app = builder.Build();
app.MapNmxOAuthConfig();
app.MapHub<BrHub>(SignalRPath.HubWeave);

// Single-instance addon — apply migrations on startup, no rolling deploy concern.
using (var scope = app.Services.CreateScope())
{
    await using var db = await scope.ServiceProvider
        .GetRequiredService<IDbContextFactory<WeaveDbContext>>()
        .CreateDbContextAsync();
    db.Database.Migrate();
}

app.Run();
