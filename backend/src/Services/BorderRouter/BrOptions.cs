using System.ComponentModel.DataAnnotations;

namespace Namorix.Weave.Services.BorderRouter;

public sealed class BrOptions
{
    public const string SectionName = "BorderRouter";

    [Required]
    public string Host { get; set; } = "192.168.1.10";

    [Range(1, 65535)]
    public int Port { get; set; } = 5000;

    // Must stay well under the firmware state watchdog (5×15s restart).
    [Range(1, 60)]
    public int StatePollIntervalSec { get; set; } = 5;

    [Range(500, 30000)]
    public int RequestTimeoutMs { get; set; } = 4000;

    public TimeSpan RequestTimeout => TimeSpan.FromMilliseconds(RequestTimeoutMs);
}
