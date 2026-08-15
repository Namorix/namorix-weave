import { useAppDispatch } from "../store/hooks"
import { networkActions } from "../store/slices/networkSlice"

import type { Network } from "../types/network"
import { useSignalR, useSignalREvent, WeaveSignalREvents } from "../signalr"

export function useNetworks(): void {
  const dispatch = useAppDispatch()
  useSignalR(true)

  useSignalREvent(WeaveSignalREvents.NetworkList, (networks: Network[]) => {
    dispatch(networkActions.setNetworkList(networks))
  })

  useSignalREvent(WeaveSignalREvents.NetworkChanged, (network: Network) => {
    dispatch(networkActions.upsertNetwork(network))
  })
}
