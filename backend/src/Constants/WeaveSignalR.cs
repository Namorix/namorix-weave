namespace Namorix.Weave.Constants;

public static class SignalRPath
{
    public const string HubPrefix = "/hubs";
    public const string HubWeave = $"{HubPrefix}/weave";
}

public static class WeaveSignalRGroups
{
    public const string Network = "network";
    public const string BorderRouter = "border-router";
}

public static class WeaveSignalREvents
{
    // Network (protocol-agnostic)
    public const string NetworkListChanged = $"{WeaveSignalRGroups.Network}:list-changed";
    public const string NetworkChanged = $"{WeaveSignalRGroups.Network}:changed";

    // Border router (BR/Thread-specific live data)
    public const string BrConnection = $"{WeaveSignalRGroups.BorderRouter}:connection-changed";
    public const string BrState = $"{WeaveSignalRGroups.BorderRouter}:state-changed";
    public const string BrHealth = $"{WeaveSignalRGroups.BorderRouter}:health-changed";
    public const string BrDataset = $"{WeaveSignalRGroups.BorderRouter}:dataset-changed";
    public const string BrRouterTable = $"{WeaveSignalRGroups.BorderRouter}:router-table-changed";
    public const string BrChildTable = $"{WeaveSignalRGroups.BorderRouter}:child-table-changed";
    public const string BrJoinerTable = $"{WeaveSignalRGroups.BorderRouter}:joiner-table-changed";
}
