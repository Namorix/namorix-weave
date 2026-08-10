import type { WeaveSignalRGroupsType, WeaveSignalREvensType } from "./constants"
import { coreConfig } from "../config/coreConfig"

export const {
  useSignalR,
  useSignalRStatus,
  useSignalRGroup,
  useSignalREvent,
} = coreConfig.signalRHooks

export function useServerSignalRGroup(
  groupName: WeaveSignalRGroupsType,
  active: boolean,
) {
  return useSignalRGroup<WeaveSignalRGroupsType>(groupName, active)
}

export function useServerSignalREvent<T = unknown>(
  eventName: WeaveSignalREvensType,
  handler: (data: T) => void,
) {
  return useSignalREvent<T, WeaveSignalREvensType>(eventName, handler)
}
