using Namorix.Weave.Dtos;

namespace Namorix.Weave.Infrastructure;

public interface INetworkNotifier
{
    Task NotifyListChanged();
    Task NotifyChanged(NetworkDto network);
}
