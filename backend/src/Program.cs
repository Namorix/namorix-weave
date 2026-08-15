using Microsoft.AspNetCore.DataProtection;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Options;
using Namorix.Core.AddonSession;
using Namorix.Core.Grpc;
using Namorix.Core.IO;
using Namorix.Core.OAuth;
using Namorix.Weave.BorderRouter;
using Namorix.Weave.Constants;
using Namorix.Weave.Hubs;
using Namorix.Weave.Persistence;
using Namorix.Weave.Services;
using Namorix.Weave.Services.BorderRouter;
using Yarp.ReverseProxy.Configuration;
using Yarp.ReverseProxy.Forwarder;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddNmxOAuth2Client();
builder.Services.AddAddonChannelClient();
builder.Services.AddHostedService<WeaveService>();

builder.Services.AddSignalR();
builder.Services.AddMemoryCache();

if (builder.Environment.IsDevelopment())
{
    var viteHost = builder.Configuration.GetValue("Frontend:Host", "http://localhost") ?? "http://localhost";
    var vitePort = builder.Configuration.GetValue("Frontend:Port", 5102);
    var viteUrl = $"{viteHost}:{vitePort}";

    // Dev-only single-origin: Kestrel (5100) is the sole entry — anything that
    // isn't an API/hub/.well-known endpoint (assets, @vite/client, HMR websocket)
    // forwards to the live Vite server.
    builder.Services.AddReverseProxy().LoadFromMemory(
    [
        new RouteConfig
        {
            RouteId = "dev:vite",
            ClusterId = "dev:vite",
            Match = new RouteMatch { Hosts = ["localhost", "127.0.0.1"], Path = "{**catch-all}" }
        }
    ],
    [
        new ClusterConfig
        {
            ClusterId = "dev:vite",
            Destinations = new Dictionary<string, DestinationConfig>
            {
                ["default"] = new() { Address = viteUrl }
            },
            // Vite cold start (esbuild dep pre-bundling) can exceed YARP's default
            // 100s ActivityTimeout; raise it so the first transform isn't cut.
            HttpRequest = new ForwarderRequestConfig
            {
                ActivityTimeout = TimeSpan.FromMinutes(10)
            }
        }
    ]);
}
else
{
    builder.Services.AddReverseProxy();
}

builder.Services.AddAddonSessionAuth<WeaveDbContext>(o =>
    builder.Configuration.GetSection(AddonSessionAuthOptions.SectionName).Bind(o));
builder.Services.AddOptions<BrOptions>()
    .Bind(builder.Configuration.GetSection(BrOptions.SectionName))
    .ValidateDataAnnotations()
    .ValidateOnStart();

// Persistence: DB lives alongside oauth.json under the addon DataDir (NMX_DATA_DIR).
// Weave already references Namorix.Core, so DataDirectory gives the resolved base path.
var addon = NmxAddonConfig.FromEnvironment();
builder.Services.AddSingleton(new DataDirectory(addon.DataDir));
// DataProtection keyring stays inside the addon DataDir — independent of the desktop's keyring.
builder.Services.AddDataProtection()
    .PersistKeysToFileSystem(new DirectoryInfo(Path.Combine(addon.DataDir, "keys")));
builder.Services.AddSingleton<WeaveSecretProtector>();

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
        sp.GetRequiredService<WeaveSecretProtector>(),
        sp.GetRequiredService<ILogger<BrProvisioningService>>());
});

builder.Services.AddHostedService<BrConnectionService>();

var app = builder.Build();

// Chrome DevTools polls this path on page load; 404 it before session auth / YARP
// so it neither triggers a session DB lookup nor gets proxied to the Vite dev server.
app.Map("/.well-known/appspecific/com.chrome.devtools.json", static appBuilder =>
{
    appBuilder.Run(static context =>
    {
        context.Response.StatusCode = StatusCodes.Status404NotFound;
        return Task.CompletedTask;
    });
});

// No /.well-known/nmx-oauth-config: addon auth is backend-mediated only (gRPC addon channel),
// so core createMount's browser OAuth (desktop token endpoints → nmx_addon_refresh_token) is never triggered.
app.UseAddonSessionAuth();
app.MapControllers();
app.MapHub<WeaveHub>(SignalRPath.HubWeave);
app.MapReverseProxy();

// Single-instance addon — apply migrations on startup, no rolling deploy concern.
using (var scope = app.Services.CreateScope())
{
    await using var db = await scope.ServiceProvider
        .GetRequiredService<IDbContextFactory<WeaveDbContext>>()
        .CreateDbContextAsync();
    db.Database.Migrate();
}

app.Run();
