using Microsoft.AspNetCore.DataProtection;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Options;
using Namorix.Core.AddonSession;
using Namorix.Core.Extensions;
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
builder.Services.AddMemoryCache();

builder.Services.AddDevViteReverseProxy(builder.Environment, builder.Configuration);

builder.Services.AddAddonSessionAuth<WeaveDbContext>(o =>
    builder.Configuration.GetSection(AddonSessionAuthOptions.SectionName).Bind(o));
builder.Services.AddOptions<BrOptions>()
    .Bind(builder.Configuration.GetSection(BrOptions.SectionName))
    .ValidateDataAnnotations()
    .ValidateOnStart();

var addon = NmxAddonConfig.FromEnvironment();
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

// Singleton so WeaveHub can resolve the same instance DI hosts as a hosted service.
builder.Services.AddSingleton<BrConnectionService>();
builder.Services.AddHostedService(sp => sp.GetRequiredService<BrConnectionService>());

var app = builder.Build();

app.UseChromeDevToolsProbe404();
app.UseAddonSessionAuth();
app.MapControllers();
app.MapHub<WeaveHub>(SignalRPath.HubWeave);
app.MapDevViteReverseProxy(builder.Environment);

using (var scope = app.Services.CreateScope())
{
    await using var db = await scope.ServiceProvider
        .GetRequiredService<IDbContextFactory<WeaveDbContext>>()
        .CreateDbContextAsync();
    db.Database.Migrate();
}

app.Run();
