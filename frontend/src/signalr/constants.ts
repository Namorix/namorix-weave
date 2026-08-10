export const WeaveSignalRGroups = {
  BorderRouter: "border-router",
} as const

export const WeaveSignalREvents = {
  BrConnection: WeaveSignalRGroups.BorderRouter + ":connection",
  BrState: WeaveSignalRGroups.BorderRouter + ":state",
  BrHealth: WeaveSignalRGroups.BorderRouter + ":health",
  BrDataset: WeaveSignalRGroups.BorderRouter + ":dataset",
  BrRouterTable: WeaveSignalRGroups.BorderRouter + ":router-table",
  BrChildTable: WeaveSignalRGroups.BorderRouter + ":child-table",
  BrJoinerTable: WeaveSignalRGroups.BorderRouter + ":joiner-table",
} as const

export type WeaveSignalRGroupsType =
  (typeof WeaveSignalRGroups)[keyof typeof WeaveSignalRGroups]
export type WeaveSignalREvensType =
  (typeof WeaveSignalREvents)[keyof typeof WeaveSignalREvents]
