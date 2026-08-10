import { createSlice, type PayloadAction } from "@reduxjs/toolkit"
import type {
  BrConnectionStatus,
  ChildEntry,
  JoinerEntry,
  OtbrStatus,
  RouterEntry,
  ThreadDataset,
  ThreadRole,
} from "../../types/network"

interface NormalizedTable<T extends { id: string }> {
  byId: Record<string, T>
  order: string[]
}

function toNormalizedTable<T extends { id: string }>(
  entries: readonly T[],
): NormalizedTable<T> {
  const byId: Record<string, T> = {}
  const order: string[] = []

  for (const entry of entries) {
    byId[entry.id] = entry
    order.push(entry.id)
  }

  return { byId, order }
}

interface NetworkState {
  connection: BrConnectionStatus
  role: ThreadRole
  ipAddress: string | null
  threadVersion: string | null
  dataset: ThreadDataset | null
  routerTable: NormalizedTable<RouterEntry>
  childTable: NormalizedTable<ChildEntry>
  joinerTable: NormalizedTable<JoinerEntry>
  loading: boolean
  error: string | null
}

const initialState: NetworkState = {
  connection: { isConnected: false, host: null, port: null },
  role: "disabled",
  ipAddress: null,
  threadVersion: null,
  dataset: null,
  routerTable: { byId: {}, order: [] },
  childTable: { byId: {}, order: [] },
  joinerTable: { byId: {}, order: [] },
  loading: false,
  error: null,
}

const slice = createSlice({
  name: "network",
  initialState,
  reducers: {
    setOtbrStatus(state, action: PayloadAction<OtbrStatus>) {
      const { connection, role, ipAddress, threadVersion } = action.payload
      state.connection = connection
      state.role = role
      state.ipAddress = ipAddress
      state.threadVersion = threadVersion
    },
    setThreadDataset(state, action: PayloadAction<ThreadDataset | null>) {
      state.dataset = action.payload
    },
    setRouterTable(state, action: PayloadAction<RouterEntry[]>) {
      state.routerTable = toNormalizedTable(action.payload)
    },
    setChildTable(state, action: PayloadAction<ChildEntry[]>) {
      state.childTable = toNormalizedTable(action.payload)
    },
    setJoinerTable(state, action: PayloadAction<JoinerEntry[]>) {
      state.joinerTable = toNormalizedTable(action.payload)
    },
    setLoading(state, action: PayloadAction<boolean>) {
      state.loading = action.payload
    },
    setError(state, action: PayloadAction<string | null>) {
      state.error = action.payload
    },
  },
})

export const networkActions = slice.actions
export const networkReducer = slice.reducer
