using System;

namespace Namorix.Weave.BorderRouter.Models;

// Mirrors ot_change_mask_t in firmware ot_change_detector.h (u32 big-endian).
[Flags]
public enum BrChangedMask : uint
{
    None = 0,
    Role = 1u << 0,
    Ip = 1u << 1,
    Dataset = 1u << 2,
    RouterTable = 1u << 3,
    ChildTable = 1u << 4,
    JoinerTable = 1u << 5,
}
