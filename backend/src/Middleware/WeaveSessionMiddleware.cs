using System.Security.Claims;
using Namorix.Weave.Services;

namespace Namorix.Weave.Middleware;

public sealed class WeaveSessionMiddleware(
    RequestDelegate next,
    AddonSessionService sessions,
    WeaveOAuthService oauth,
    ILogger<WeaveSessionMiddleware> logger)
{
    public async Task InvokeAsync(HttpContext context)
    {
        if (!context.Request.Cookies.TryGetValue(AddonSessionService.CookieName, out var sessionId))
        {
            await next(context);
            return;
        }

        var session = await sessions.FindAsync(sessionId, context.RequestAborted);

        if (session is not null && session.AccessTokenExpiresAt <= DateTime.UtcNow)
        {
            if (session.RefreshTokenExpiresAt <= DateTime.UtcNow)
            {
                await sessions.DeleteAsync(sessionId, context.RequestAborted);
                session = null;
            }
            else
            {
                try
                {
                    await oauth.RefreshSessionAsync(session, context.RequestAborted);
                }
                catch (Exception ex)
                {
                    logger.LogWarning(ex,
                        "Silent refresh failed for session {SessionId}; removing session", sessionId);
                    await sessions.DeleteAsync(sessionId, context.RequestAborted);
                    session = null;
                }
            }
        }

        if (session is not null)
        {
            var userId = session.UserId.ToString();
            var identity = new ClaimsIdentity(
            [
                new Claim(ClaimTypes.NameIdentifier, userId),
                new Claim(ClaimTypes.Name, userId),
                new Claim("session_id", session.Id),
                new Claim("client_id", session.ClientId),
            ], "WeaveSession");

            context.User = new ClaimsPrincipal(identity);
        }

        await next(context);
    }
}
