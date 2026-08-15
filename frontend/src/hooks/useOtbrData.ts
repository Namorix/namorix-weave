import { useCallback, useEffect } from "react"

import { useAppDispatch } from "../store/hooks"
import { networkActions } from "../store/slices/networkSlice"
import { networkController } from "../controllers/network.controller"

import type { BrConnectionChanged, BrNotify } from "../types/network"
import {
  useServerSignalRGroup,
  useSignalR,
  useSignalREvent,
  WeaveSignalREvents,
  WeaveSignalRGroups,
} from "../signalr"

export function useOtbrData(networkId: number): void {
  const dispatch = useAppDispatch()
  useSignalR(true)
  useServerSignalRGroup(WeaveSignalRGroups.BorderRouter, true)

  const fetchState = useCallback(async () => {
    try {
      const state = await networkController.getBrState(networkId)
      dispatch(networkActions.setOtbrStatus(state))
    } catch {
      dispatch(
        networkActions.setConnection({ isConnected: false, host: null, port: null }),
      )
    }
  }, [networkId, dispatch])

  const fetchDataset = useCallback(async () => {
    try {
      const dataset = await networkController.getBrDataset(networkId)
      dispatch(networkActions.setThreadDataset(dataset))
    } catch {
      dispatch(networkActions.setThreadDataset(null))
    }
  }, [networkId, dispatch])

  const fetchTables = useCallback(async () => {
    try {
      const tables = await networkController.getBrTables(networkId)
      dispatch(networkActions.setRouterTable(tables.router))
      dispatch(networkActions.setChildTable(tables.child))
      dispatch(networkActions.setJoinerTable(tables.joiner))
    } catch {
      dispatch(networkActions.setRouterTable([]))
      dispatch(networkActions.setChildTable([]))
      dispatch(networkActions.setJoinerTable([]))
    }
  }, [networkId, dispatch])

  useEffect(() => {
    let cancelled = false
    dispatch(networkActions.setLoading(true))
    void Promise.all([fetchState(), fetchDataset(), fetchTables()]).finally(
      () => {
        if (!cancelled) dispatch(networkActions.setLoading(false))
      },
    )
    return () => {
      cancelled = true
    }
  }, [fetchState, fetchDataset, fetchTables, dispatch])

  useSignalREvent(WeaveSignalREvents.BrConnection, (s: BrConnectionChanged) => {
    if (s.networkId !== networkId) return
    dispatch(
      networkActions.setConnection({
        isConnected: s.isConnected,
        host: s.host,
        port: s.port,
      }),
    )
    if (s.isConnected) {
      void Promise.all([fetchState(), fetchDataset(), fetchTables()])
    }
  })

  useSignalREvent(WeaveSignalREvents.BrState, (n: BrNotify) => {
    if (n.networkId !== networkId) return
    void fetchState()
  })

  useSignalREvent(WeaveSignalREvents.BrDataset, (n: BrNotify) => {
    if (n.networkId !== networkId) return
    void fetchDataset()
  })

  useSignalREvent(WeaveSignalREvents.BrRouterTable, (n: BrNotify) => {
    if (n.networkId !== networkId) return
    void fetchTables()
  })

  useSignalREvent(WeaveSignalREvents.BrChildTable, (n: BrNotify) => {
    if (n.networkId !== networkId) return
    void fetchTables()
  })

  useSignalREvent(WeaveSignalREvents.BrJoinerTable, (n: BrNotify) => {
    if (n.networkId !== networkId) return
    void fetchTables()
  })
}
