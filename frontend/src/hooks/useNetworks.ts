import { useEffect } from "react"

import { useAppDispatch } from "../store/hooks"
import { networkActions } from "../store/slices/networkSlice"
import { networkController } from "../controllers/network.controller"

import type { Network } from "../types/network"
import {
  useServerSignalRGroup,
  useSignalR,
  useSignalREvent,
  WeaveSignalREvents,
  WeaveSignalRGroups,
} from "../signalr"

export function useNetworks(): void {
  const dispatch = useAppDispatch()
  useSignalR(true)
  useServerSignalRGroup(WeaveSignalRGroups.Network, true)

  useEffect(() => {
    let cancelled = false
    void networkController
      .list()
      .then((networks) => {
        if (!cancelled) dispatch(networkActions.setNetworkList(networks))
      })
      .catch(() => {})
    return () => {
      cancelled = true
    }
  }, [dispatch])

  useSignalREvent(WeaveSignalREvents.NetworkListChanged, () => {
    void networkController
      .list()
      .then((networks) => dispatch(networkActions.setNetworkList(networks)))
      .catch(() => {})
  })

  useSignalREvent(WeaveSignalREvents.NetworkChanged, (network: Network) => {
    dispatch(networkActions.upsertNetwork(network))
  })
}
