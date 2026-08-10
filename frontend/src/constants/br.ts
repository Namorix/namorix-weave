export const HUB_WEAVE = "/hubs/weave"

export const BrEvents = {
  connection: "weave:br-connection",
  state: "weave:br-state",
  health: "weave:br-health",
  dataset: "weave:dataset",
  routerTable: "weave:router-table",
  childTable: "weave:child-table",
  joinerTable: "weave:joiner-table",
} as const
