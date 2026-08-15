import { API_BASE } from "@namorix/core"

export const API_NETWORKS_BASE = API_BASE + "/networks"

export const WeaveApiRoutes = {
  networks: API_NETWORKS_BASE,
  networkById: (id: number) => `${API_NETWORKS_BASE}/${id}`,
  networkAccept: (id: number) => `${API_NETWORKS_BASE}/${id}/accept`,
  networkReject: (id: number) => `${API_NETWORKS_BASE}/${id}/reject`,
  brState: (id: number) => `${API_NETWORKS_BASE}/${id}/br/state`,
  brDataset: (id: number) => `${API_NETWORKS_BASE}/${id}/br/dataset`,
  brTables: (id: number) => `${API_NETWORKS_BASE}/${id}/br/tables`,
} as const
