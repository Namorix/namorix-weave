namespace Namorix.Weave.Infrastructure;

public interface IBrNotifier
{
    Task NotifyConnectionChanged(int networkId, bool isConnected, string? host, int? port);
    Task NotifyStateChanged(int networkId);
    Task NotifyHealthChanged(int networkId);
    Task NotifyDatasetChanged(int networkId);
    Task NotifyRouterTableChanged(int networkId);
    Task NotifyChildTableChanged(int networkId);
    Task NotifyJoinerTableChanged(int networkId);
}
