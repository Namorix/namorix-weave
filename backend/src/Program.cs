using Namorix.Core.Grpc;
using Namorix.Core.OAuth;
using Namorix.Weave.Services;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddNmxOAuth2Client();
builder.Services.AddAddonChannelClient();
builder.Services.AddHostedService<WeaveService>();

var app = builder.Build();
app.MapNmxOAuthConfig();
app.Run();
