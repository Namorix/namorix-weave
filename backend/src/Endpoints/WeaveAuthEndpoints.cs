using Namorix.Weave.Services;

namespace Namorix.Weave.Endpoints;

public static class WeaveAuthEndpoints
{
    public static void MapWeaveAuthEndpoints(this WebApplication app)
    {
        app.MapGet("/api/oauth/login", async (WeaveOAuthService oauth, HttpRequest request, CancellationToken ct) =>
            Results.Redirect(await oauth.BuildLoginUrlAsync(request, ct)));

        app.MapGet("/api/oauth/callback", async (
            string code, string state,
            WeaveOAuthService oauth, AddonSessionService sessions,
            HttpResponse response, CancellationToken ct) =>
        {
            try
            {
                var session = await oauth.CompleteLoginAsync(code, state, ct);
                response.Cookies.Append(AddonSessionService.CookieName, session.Id,
                    new CookieOptions
                    {
                        HttpOnly = true,
                        SameSite = SameSiteMode.Lax,
                        Path = "/",
                        MaxAge = TimeSpan.FromDays(30),
                    });
                return Results.Redirect("/");
            }
            catch (InvalidOperationException ex)
            {
                return Results.BadRequest(new { error = ex.Message });
            }
        });

        app.MapGet("/api/oauth/status", (HttpContext context) =>
        {
            var userId = context.User.FindFirst(System.Security.Claims.ClaimTypes.NameIdentifier)?.Value;
            return userId is null
                ? Results.Json(new { authenticated = false }, statusCode: StatusCodes.Status401Unauthorized)
                : Results.Ok(new { authenticated = true, userId = int.Parse(userId) });
        });

        app.MapPost("/api/oauth/logout", async (
            AddonSessionService sessions, HttpContext context, HttpResponse response,
            CancellationToken ct) =>
        {
            if (context.Request.Cookies.TryGetValue(AddonSessionService.CookieName, out var sessionId))
            {
                await sessions.DeleteAsync(sessionId, ct);
                response.Cookies.Delete(AddonSessionService.CookieName,
                    new CookieOptions { Path = "/" });
            }
            return Results.NoContent();
        });
    }
}
