using Microsoft.AspNetCore.DataProtection;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Options;
using Namorix.Core.AddonSession;
using Namorix.Core.Extensions;
using Namorix.Core.Grpc;
using Namorix.Core.IO;
using Namorix.Core.OAuth;
using Namorix.Weave.BorderRouter;
using Namorix.Weave.Constants;
using Namorix.Weave.Hubs;
using Namorix.Weave.Infrastructure;
using Namorix.Weave.Persistence;
using Namorix.Weave.Services;
using Namorix.Weave.Services.BorderRouter;

var builder = WebApplication.CreateBuilder(args);

var addon = NmxAddonConfig.FromEnvironment();

// Core DI: controllers + JSON (WhenWritingNull) + flat-file logging + rate limiter + notifiers → WeaveHub
builder.Services.AddNamorixCore<WeaveHub>(builder.Environment.IsDevelopment(), o =>
{
    o.DataBasePath = addon.DataDir;
    o.HubPath = SignalRPath.HubWeave;
});

builder.Services.AddNmxOAuth2Client();
builder.Services.AddAddonChannelClient();
builder.Services.AddHostedService<WeaveService>();

builder.Services.AddDevViteReverseProxy(builder.Environment, builder.Configuration);

builder.Services.AddAddonSessionAuth<WeaveDbContext>(o =>
    builder.Configuration.GetSection(AddonSessionAuthOptions.SectionName).Bind(o));
builder.Services.AddOptions<BrOptions>()
    .Bind(builder.Configuration.GetSection(BrOptions.SectionName))
    .ValidateDataAnnotations()
    .ValidateOnStart();

var dbPath = Path.Combine(addon.DataDir, "weave.db");

builder.Services.AddSingleton(new DataDirectory(addon.DataDir));
builder.Services.AddDataProtection()
    .PersistKeysToFileSystem(new DirectoryInfo(Path.Combine(addon.DataDir, "keys")));
builder.Services.AddSingleton<WeaveSecretProtector>();

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
        sp.GetRequiredService<WeaveSecretProtector>(),
        sp.GetRequiredService<ILogger<BrProvisioningService>>());
});

// Notifiers wrap IHubContext only — stateless, singleton-safe. BrConnectionService
// (singleton hosted service) và controllers đều resolve cùng instance.
builder.Services.AddSingleton<INetworkNotifier, SignalRNetworkNotifier>();
builder.Services.AddSingleton<IBrNotifier, SignalRBrNotifier>();

// Singleton so WeaveHub can resolve the same instance DI hosts as a hosted service.
builder.Services.AddSingleton<BrConnectionService>();
builder.Services.AddHostedService(sp => sp.GetRequiredService<BrConnectionService>());

var app = builder.Build();

app.UseNamorixCore<WeaveHub>(
    configurePipeline: a =>
    {
        a.UseChromeDevToolsProbe404();
        a.UseAddonSessionAuth();
    },
    configureEndpoints: e =>
    {
        e.MapDevViteReverseProxy(builder.Environment);
    });

using (var scope = app.Services.CreateScope())
{
    await using var db = await scope.ServiceProvider
        .GetRequiredService<IDbContextFactory<WeaveDbContext>>()
        .CreateDbContextAsync();
    db.Database.Migrate();
}

app.Run();
