using System.Collections.Concurrent;
using Namorix.Weave.BorderRouter.Frame;

namespace Namorix.Weave.BorderRouter;

public sealed class PendingFrameStore
{
    private sealed record PendingEntry(TaskCompletionSource<BrFrame> Tcs, CancellationTokenSource Cts);

    private readonly ConcurrentDictionary<byte, PendingEntry> _pending = new();
    private byte _nextId;

    public byte NextId()
    {
        for (var i = 0; i < 256; i++)
        {
            _nextId = (byte)((_nextId + 1) & 0xFF);
            if (!_pending.ContainsKey(_nextId))
                return _nextId;
        }
        throw new InvalidOperationException("No free frame ID.");
    }

    public Task<BrFrame> Register(byte frameId, TimeSpan timeout)
    {
        var tcs = new TaskCompletionSource<BrFrame>(TaskCreationOptions.RunContinuationsAsynchronously);
        var cts = new CancellationTokenSource();
        var entry = new PendingEntry(tcs, cts);

        if (!_pending.TryAdd(frameId, entry))
            throw new InvalidOperationException($"Frame {frameId} already pending.");

        cts.CancelAfter(timeout);
        cts.Token.Register(() => TryFail(frameId, new TimeoutException($"Frame {frameId} timed out after {timeout}.")));
        return tcs.Task;
    }

    public bool TryComplete(byte frameId, BrFrame frame)
    {
        if (!_pending.TryRemove(frameId, out var entry))
            return false;

        entry.Cts.Dispose();
        entry.Tcs.TrySetResult(frame);
        return true;
    }

    public bool TryFail(byte frameId, Exception exception)
    {
        if (!_pending.TryRemove(frameId, out var entry))
            return false;

        entry.Cts.Dispose();
        entry.Tcs.TrySetException(exception);
        return true;
    }

    public void FailAll(Exception exception)
    {
        foreach (var frameId in _pending.Keys.ToArray())
            TryFail(frameId, exception);
    }
}
