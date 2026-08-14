using System.ComponentModel.DataAnnotations;

namespace Namorix.Weave.Services.BorderRouter;

public sealed class BrOptions
{
    public const string SectionName = "BorderRouter";

    public string MdnsServiceName { get; set; } = "_thread-border-router-frame._tcp";

    // Fallback port when the mDNS SRV record is missing its port.
    [Range(1, 65535)]
    public int FramePort { get; set; } = 5150;

    // Must stay well under the firmware state watchdog (5×15s restart).
    [Range(1, 60)]
    public int StatePollIntervalSec { get; set; } = 5;

    [Range(500, 30000)]
    public int RequestTimeoutMs { get; set; } = 4000;

    public TimeSpan RequestTimeout => TimeSpan.FromMilliseconds(RequestTimeoutMs);
}
