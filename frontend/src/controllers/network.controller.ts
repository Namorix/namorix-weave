import { ApiError } from "@namorix/core"
import { coreConfig } from "../config/coreConfig"
import type { Network, NetworkAcceptRequest } from "../types/network"

async function acceptNetwork(
  networkId: number,
  request: NetworkAcceptRequest,
): Promise<Network> {
  const data = await coreConfig.http
    .url(`${coreConfig.getApiBaseUrl()}/api/networks/${networkId}/accept`)
    .post(request)
    .json<Network>()
  if (!data.success) throw ApiError.fromResponse(data)
  return data.data
}

async function rejectNetwork(networkId: number): Promise<Network> {
  const data = await coreConfig.http
    .url(`${coreConfig.getApiBaseUrl()}/api/networks/${networkId}/reject`)
    .post()
    .json<Network>()
  if (!data.success) throw ApiError.fromResponse(data)
  return data.data
}

export const networkController = { acceptNetwork, rejectNetwork }
