using System.Net.Sockets;
using Namorix.Weave.BorderRouter.Frame;

namespace Namorix.Weave.BorderRouter;

public sealed class BrTcpClient(string host, int port, ILogger<BrTcpClient> logger) : IAsyncDisposable
{
    private readonly PendingFrameStore _pending = new();
    private readonly SemaphoreSlim _writeLock = new(1, 1);
    private readonly List<byte> _receiveBuffer = new();

    private volatile TcpClient? _client;
    private CancellationTokenSource? _cts;
    private Task? _runTask;

    public event Action<BrFrame>? FrameReceived;
    public event Action? Connected;
    public event Action? Disconnected;

    public bool IsConnected => _client is not null;

    public void Start(CancellationToken ct)
    {
        _cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        _runTask = Task.Run(() => RunLoopAsync(_cts.Token), CancellationToken.None);
    }

    public async Task<BrFrame> RequestAsync(byte command, byte[] payload, TimeSpan timeout, CancellationToken ct = default)
    {
        var frameId = _pending.NextId();
        var task = _pending.Register(frameId, timeout);
        await SendFrameAsync(new BrFrame(frameId, command, payload), ct);
        return await task;
    }

    public async Task SendFrameAsync(BrFrame frame, CancellationToken ct = default)
    {
        var encoded = FrameCodec.Encode(frame);

        await _writeLock.WaitAsync(ct);
        try
        {
            var client = _client;
            if (client is null)
                throw new IOException("BR client is not connected.");

            await client.GetStream().WriteAsync(encoded, ct);
        }
        finally
        {
            _writeLock.Release();
        }
    }

    private async Task RunLoopAsync(CancellationToken ct)
    {
        var attempt = 0;
        while (!ct.IsCancellationRequested)
        {
            try
            {
                await ConnectAndReadAsync(ct);
                attempt = 0;
            }
            catch (OperationCanceledException) when (ct.IsCancellationRequested)
            {
                break;
            }
            catch (Exception ex)
            {
                logger.LogWarning(ex, "BR connection failed.");
            }

            Disconnected?.Invoke();

            var delay = BackoffDelay(attempt);
            attempt++;
            logger.LogInformation("Reconnecting to {Host}:{Port} in {Delay}...", host, port, delay);

            try
            {
                await Task.Delay(delay, ct);
            }
            catch (OperationCanceledException)
            {
                break;
            }
        }
    }

    private async Task ConnectAndReadAsync(CancellationToken ct)
    {
        var client = new TcpClient();
        await client.ConnectAsync(host, port, ct);
        _client = client;
        logger.LogInformation("Connected to BR {Host}:{Port}.", host, port);
        Connected?.Invoke();

        var stream = client.GetStream();
        var buffer = new byte[4096];

        try
        {
            while (!ct.IsCancellationRequested)
            {
                var read = await stream.ReadAsync(buffer, ct);
                if (read == 0)
                    break;

                _receiveBuffer.AddRange(buffer.AsSpan(0, read));
                while (FrameCodec.TryParseNext(_receiveBuffer, out var frame))
                    Dispatch(frame!);
            }
        }
        finally
        {
            client.Close();
            _client = null;
            _receiveBuffer.Clear();
            _pending.FailAll(new IOException("BR connection lost."));
        }
    }

    private void Dispatch(BrFrame frame)
    {
        if (_pending.TryComplete(frame.FrameId, frame))
            return;

        FrameReceived?.Invoke(frame);
    }

    private static TimeSpan BackoffDelay(int attempt)
    {
        var seconds = Math.Min(30, 1 << Math.Min(attempt, 5));
        return TimeSpan.FromSeconds(seconds);
    }

    public async ValueTask DisposeAsync()
    {
        _cts?.Cancel();
        if (_runTask is not null)
        {
            try
            {
                await _runTask;
            }
            catch
            {
                // RunLoopAsync exits via OperationCanceledException on shutdown.
            }
        }

        _cts?.Dispose();
        _writeLock.Dispose();
    }
}
