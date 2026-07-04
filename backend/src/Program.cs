using Namorix.Core.OAuth;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddNmxOAuth2Client();

var app = builder.Build();

app.MapGet("/", () => Results.Ok(new { name = "namorix-weave", status = "running" }));
app.MapGet("/test-oauth", async (NmxOAuth2Client oauth) =>
{
    Console.WriteLine("Test ouath");
    var token = await oauth.GetAccessTokenAsync();
    return Results.Ok(new { access_token = token });
});

app.Run();
