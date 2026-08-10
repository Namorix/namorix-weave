namespace Namorix.Weave.BorderRouter.Models;

public sealed record BrHealthTask(string Name, uint HighWaterMarkBytes, uint StackSizeBytes);

public sealed record BrHealth(
    uint FreeHeapBytes,
    uint MinFreeHeapBytes,
    uint UptimeMs,
    uint MleDetachCount,
    IReadOnlyList<BrHealthTask> Tasks);
