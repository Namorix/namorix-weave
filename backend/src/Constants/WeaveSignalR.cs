namespace Namorix.Weave.Constants;

public static class SignalRPath
{
    public const string HubPrefix = "/hubs";
    public const string HubWeave = $"{HubPrefix}/weave";
}

public static class WeaveSignalRGroups
{
    public const string BorderRouter = $"border-router";
}

public static class WeaveSignalREvents
{
    public const string BrConnection = $"{WeaveSignalRGroups.BorderRouter}:connection";
    public const string NetworkList = $"{WeaveSignalRGroups.BorderRouter}:network-list";
    public const string NetworkChanged = $"{WeaveSignalRGroups.BorderRouter}:network-changed";
    public const string BrState = $"{WeaveSignalRGroups.BorderRouter}:state";
    public const string BrHealth = $"{WeaveSignalRGroups.BorderRouter}:health";
    public const string BrDataset = $"{WeaveSignalRGroups.BorderRouter}:dataset";
    public const string BrRouterTable = $"{WeaveSignalRGroups.BorderRouter}:router-table";
    public const string BrChildTable = $"{WeaveSignalRGroups.BorderRouter}:child-table";
    public const string BrJoinerTable = $"{WeaveSignalRGroups.BorderRouter}:joiner-table";
}