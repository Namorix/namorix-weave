using Namorix.Weave.Middleware;

namespace Namorix.Weave.Extensions;

public static class ApplicationBuilderExtensions
{
    public static IApplicationBuilder UseWeaveSessionAuth(this IApplicationBuilder app)
        => app.UseMiddleware<WeaveSessionMiddleware>();
}
