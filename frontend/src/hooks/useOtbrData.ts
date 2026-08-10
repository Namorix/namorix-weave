import { useEffect } from "react"

import { useAppDispatch } from "../store/hooks"
import { networkActions } from "../store/slices/networkSlice"

import type {
  BrConnectionStatus,
  ChildEntry,
  JoinerEntry,
  OtbrStatus,
  RouterEntry,
  ThreadDataset,
} from "../types/network"
import {
  useSignalR,
  useSignalREvent,
  useSignalRStatus,
  WeaveSignalREvents,
} from "../signalr"

export function useOtbrData(): void {
  const dispatch = useAppDispatch()
  const status = useSignalRStatus()
  useSignalR(true)

  useEffect(() => {
    dispatch(networkActions.setLoading(true))
  }, [dispatch])

  useEffect(() => {
    if (status === "connected") {
      dispatch(networkActions.setLoading(false))
    } else if (status === "disconnected") {
      dispatch(
        networkActions.setConnection({
          isConnected: false,
          host: null,
          port: null,
        }),
      )
    }
  }, [status, dispatch])

  useSignalREvent(WeaveSignalREvents.BrConnection, (s: BrConnectionStatus) => {
    dispatch(networkActions.setConnection(s))
  })

  useSignalREvent(WeaveSignalREvents.BrState, (s: OtbrStatus) => {
    dispatch(networkActions.setOtbrStatus(s))
    dispatch(networkActions.setLoading(false))
  })

  useSignalREvent(WeaveSignalREvents.BrDataset, (d: ThreadDataset | null) => {
    dispatch(networkActions.setThreadDataset(d))
  })

  useSignalREvent(
    WeaveSignalREvents.BrRouterTable,
    (entries: RouterEntry[]) => {
      dispatch(networkActions.setRouterTable(entries))
    },
  )

  useSignalREvent(WeaveSignalREvents.BrChildTable, (entries: ChildEntry[]) => {
    dispatch(networkActions.setChildTable(entries))
  })

  useSignalREvent(
    WeaveSignalREvents.BrJoinerTable,
    (entries: JoinerEntry[]) => {
      dispatch(networkActions.setJoinerTable(entries))
    },
  )
}
