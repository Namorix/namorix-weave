namespace Namorix.Weave.Models;

public enum Protocol
{
    Thread = 0,
    Zigbee = 1,
}

public enum NetworkStatus
{
    Pending = 0,
    Connected = 1,
    Offline = 2,
    Rejected = 3,
}
