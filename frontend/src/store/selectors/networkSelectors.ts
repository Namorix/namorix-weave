import { createSelector } from "@reduxjs/toolkit"
import type { RootState } from "../store"
import type {
  ChildEntry,
  JoinerEntry,
  Network,
  NetworkStatus,
  RouterEntry,
} from "../../types/network"

const selectNetwork = (state: RootState) => state.network

export const selectConnection = createSelector(
  selectNetwork,
  (network) => network.connection,
)

export const selectThreadRole = createSelector(
  selectNetwork,
  (network) => network.role,
)

export const selectIpAddress = createSelector(
  selectNetwork,
  (network) => network.ipAddress,
)

export const selectThreadVersion = createSelector(
  selectNetwork,
  (network) => network.threadVersion,
)

export const selectThreadDataset = createSelector(
  selectNetwork,
  (network) => network.dataset,
)

export const selectNetworkError = createSelector(
  selectNetwork,
  (network) => network.error,
)

export const selectRouterEntries = createSelector(
  (state: RootState) => state.network.routerTable,
  (table) =>
    table.order
      .map((id) => table.byId[id])
      .filter((entry): entry is RouterEntry => entry != null),
)

export const selectChildEntries = createSelector(
  (state: RootState) => state.network.childTable,
  (table) =>
    table.order
      .map((id) => table.byId[id])
      .filter((entry): entry is ChildEntry => entry != null),
)

export const selectJoinerEntries = createSelector(
  (state: RootState) => state.network.joinerTable,
  (table) =>
    table.order
      .map((id) => table.byId[id])
      .filter((entry): entry is JoinerEntry => entry != null),
)

export const selectNetworks = createSelector(
  (state: RootState) => state.network.devices,
  (table) =>
    table.order
      .map((id) => table.byId[id])
      .filter((network): network is Network => network != null),
)

export const selectNetworkCountByStatus = createSelector(
  selectNetworks,
  (networks) => {
    const counts: Record<NetworkStatus, number> = {
      pending: 0,
      connected: 0,
      offline: 0,
      rejected: 0,
    }
    for (const network of networks) counts[network.status] += 1
    return counts
  },
)
