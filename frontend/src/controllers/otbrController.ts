import { getSignalrClient, type SignalRStatus } from "@namorix/core"
import type { AppDispatch } from "../store/store"
import { networkActions } from "../store/slices/networkSlice"
import { BrEvents, HUB_WEAVE } from "../constants/br"
import type {
  BrConnectionStatus,
  ChildEntry,
  JoinerEntry,
  OtbrStatus,
  RouterEntry,
  ThreadDataset,
} from "../types/network"

export function startOtbrDataFeed(dispatch: AppDispatch): () => void {
  dispatch(networkActions.setLoading(true))
  dispatch(networkActions.setError(null))

  const client = getSignalrClient(HUB_WEAVE)

  client.on(BrEvents.connection, (status: BrConnectionStatus) => {
    dispatch(networkActions.setConnection(status))
  })

  client.on(BrEvents.state, (status: OtbrStatus) => {
    dispatch(networkActions.setOtbrStatus(status))
    dispatch(networkActions.setLoading(false))
  })

  client.on(BrEvents.dataset, (dataset: ThreadDataset | null) => {
    dispatch(networkActions.setThreadDataset(dataset))
  })

  client.on(BrEvents.routerTable, (entries: RouterEntry[]) => {
    dispatch(networkActions.setRouterTable(entries))
  })

  client.on(BrEvents.childTable, (entries: ChildEntry[]) => {
    dispatch(networkActions.setChildTable(entries))
  })

  client.on(BrEvents.joinerTable, (entries: JoinerEntry[]) => {
    dispatch(networkActions.setJoinerTable(entries))
  })

  const onStatus = (status: SignalRStatus) => {
    if (status === "connected") {
      dispatch(networkActions.setLoading(false))
    } else if (status === "disconnected") {
      dispatch(
        networkActions.setConnection({ isConnected: false, host: null, port: null }),
      )
    }
  }
  client.addStatusHandler(onStatus)

  client.start().catch((err: unknown) => {
    const message = err instanceof Error ? err.message : String(err)
    dispatch(networkActions.setError(`SignalR: ${message}`))
    dispatch(networkActions.setLoading(false))
  })

  return () => {
    client.removeStatusHandler(onStatus)
    void client.stop()
  }
}
