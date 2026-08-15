export const WeaveSignalRGroups = {
  Network: "network",
  BorderRouter: "border-router",
} as const

export const WeaveSignalREvents = {
  NetworkListChanged: WeaveSignalRGroups.Network + ":list-changed",
  NetworkChanged: WeaveSignalRGroups.Network + ":changed",
  BrConnection: WeaveSignalRGroups.BorderRouter + ":connection-changed",
  BrState: WeaveSignalRGroups.BorderRouter + ":state-changed",
  BrHealth: WeaveSignalRGroups.BorderRouter + ":health-changed",
  BrDataset: WeaveSignalRGroups.BorderRouter + ":dataset-changed",
  BrRouterTable: WeaveSignalRGroups.BorderRouter + ":router-table-changed",
  BrChildTable: WeaveSignalRGroups.BorderRouter + ":child-table-changed",
  BrJoinerTable: WeaveSignalRGroups.BorderRouter + ":joiner-table-changed",
} as const

export type WeaveSignalRGroupsType =
  (typeof WeaveSignalRGroups)[keyof typeof WeaveSignalRGroups]
export type WeaveSignalREvensType =
  (typeof WeaveSignalREvents)[keyof typeof WeaveSignalREvents]
