import { ApiError } from "@namorix/core"
import { coreConfig } from "../config/coreConfig"
import { WeaveApiRoutes } from "../weaveApiRoutes"
import type {
  BrTables,
  Network,
  NetworkAcceptRequest,
  OtbrStatus,
  ThreadDataset,
} from "../types"

async function list(): Promise<Network[]> {
  const data = await coreConfig.http
    .url(`${coreConfig.getApiBaseUrl()}${WeaveApiRoutes.networks}`)
    .get()
    .json<Network[]>()
  if (!data.success) throw ApiError.fromResponse(data)
  return data.data
}

async function getDetail(networkId: number): Promise<Network> {
  const data = await coreConfig.http
    .url(`${coreConfig.getApiBaseUrl()}${WeaveApiRoutes.networkById(networkId)}`)
    .get()
    .json<Network>()
  if (!data.success) throw ApiError.fromResponse(data)
  return data.data
}

async function acceptNetwork(
  networkId: number,
  request: NetworkAcceptRequest,
): Promise<Network> {
  const data = await coreConfig.http
    .url(`${coreConfig.getApiBaseUrl()}${WeaveApiRoutes.networkAccept(networkId)}`)
    .post(request)
    .json<Network>()
  if (!data.success) throw ApiError.fromResponse(data)
  return data.data
}

async function rejectNetwork(networkId: number): Promise<Network> {
  const data = await coreConfig.http
    .url(`${coreConfig.getApiBaseUrl()}${WeaveApiRoutes.networkReject(networkId)}`)
    .post()
    .json<Network>()
  if (!data.success) throw ApiError.fromResponse(data)
  return data.data
}

async function getBrState(networkId: number): Promise<OtbrStatus> {
  const data = await coreConfig.http
    .url(`${coreConfig.getApiBaseUrl()}${WeaveApiRoutes.brState(networkId)}`)
    .get()
    .json<OtbrStatus>()
  if (!data.success) throw ApiError.fromResponse(data)
  return data.data
}

async function getBrDataset(networkId: number): Promise<ThreadDataset> {
  const data = await coreConfig.http
    .url(`${coreConfig.getApiBaseUrl()}${WeaveApiRoutes.brDataset(networkId)}`)
    .get()
    .json<ThreadDataset>()
  if (!data.success) throw ApiError.fromResponse(data)
  return data.data
}

async function getBrTables(networkId: number): Promise<BrTables> {
  const data = await coreConfig.http
    .url(`${coreConfig.getApiBaseUrl()}${WeaveApiRoutes.brTables(networkId)}`)
    .get()
    .json<BrTables>()
  if (!data.success) throw ApiError.fromResponse(data)
  return data.data
}

export const networkController = {
  list,
  getDetail,
  acceptNetwork,
  rejectNetwork,
  getBrState,
  getBrDataset,
  getBrTables,
}
