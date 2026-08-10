using Microsoft.Extensions.Options;
using Namorix.Core.Grpc;
using Namorix.Core.OAuth;
using Namorix.Weave.BorderRouter;
using Namorix.Weave.Constants;
using Namorix.Weave.Hubs;
using Namorix.Weave.Services;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddNmxOAuth2Client();
builder.Services.AddAddonChannelClient();
builder.Services.AddHostedService<WeaveService>();

builder.Services.AddSignalR();
builder.Services.AddOptions<BorderRouterOptions>()
    .Bind(builder.Configuration.GetSection(BorderRouterOptions.SectionName))
    .ValidateDataAnnotations()
    .ValidateOnStart();

builder.Services.AddSingleton(sp =>
{
    var options = sp.GetRequiredService<IOptions<BorderRouterOptions>>().Value;
    return new BrTcpClient(options.Host, options.Port, sp.GetRequiredService<ILogger<BrTcpClient>>());
});

builder.Services.AddSingleton(sp =>
{
    var transport = sp.GetRequiredService<BrTcpClient>();
    var timeout = sp.GetRequiredService<IOptions<BorderRouterOptions>>().Value.RequestTimeout;
    return new BrCommandClient(transport, timeout);
});

builder.Services.AddHostedService<BrConnectionService>();

var app = builder.Build();
app.MapNmxOAuthConfig();
app.MapHub<BrHub>(SignalRPath.HubWeave);
app.Run();
