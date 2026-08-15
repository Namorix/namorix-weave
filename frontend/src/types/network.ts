export type ThreadRole = "disabled" | "detached" | "child" | "router" | "leader"

export interface BrConnectionStatus {
  isConnected: boolean
  host: string | null
  port: number | null
}

export interface OtbrStatus {
  connection: BrConnectionStatus
  role: ThreadRole
  ipAddress: string | null
  threadVersion: string | null
}

export interface ThreadDataset {
  activeTimestamp?: string
  networkName?: string
  channel?: number
  channelMask?: string
  extendedPanId?: string
  meshLocalPrefix?: string
  networkKey?: string
  panId?: string
  pskc?: string
  securityPolicy?: string
}

export interface RouterEntry {
  id: string
  routerId: number
  rloc16: string
  extAddress: string
  linkQualityIn: number
  linkQualityOut: number
  age: number
}

export interface ChildEntry {
  id: string
  childId: number
  rloc16: string
  extAddress: string
  linkQualityIn: number
  averageRssi: number
  fullThreadDevice: boolean
  rxOnWhenIdle: boolean
  age: number
}

export type JoinerType = "any" | "eui64" | "discerner"

export interface JoinerEntry {
  id: string
  type: JoinerType
  sharedId: string
  pskd: string
  expirationTime: number
}

export type NetworkStatus = "Pending" | "Connected" | "Offline" | "Rejected"

export interface Network {
  id: number
  protocol: string
  name?: string
  host?: string
  status: NetworkStatus
  eui64?: string
  publicKey?: string
  firstSeenAt?: string
  acceptedAt?: string
  rejectedAt?: string
}
