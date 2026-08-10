import type {
  ChildEntry,
  JoinerEntry,
  OtbrStatus,
  RouterEntry,
  ThreadDataset,
} from "../types/network"

export const OTBR_MOCK_STATUS: OtbrStatus = {
  connection: { isConnected: true, host: "192.168.1.50", port: 5000 },
  role: "leader",
  ipAddress: "fde8:50af:bc1:599::ff:fe00:fc00",
  threadVersion: "OPENTHREAD/thread-reference-20230706-",
}

export const OTBR_MOCK_DATASET: ThreadDataset = {
  activeTimestamp: "2026-08-10T08:30:00Z",
  networkName: "weave-home",
  channel: 15,
  channelMask: "0x07fff800",
  extendedPanId: "f0:f5:bd:ff:fe:10:4b:24",
  meshLocalPrefix: "fde8:50af:bc1:599::/64",
  networkKey: "aabbccddeeff00112233445566778899",
  panId: "0xabcd",
  pskc: "GSNWXK5FVJUGM2B2HOLFOBQ6DEJFX64Q",
  securityPolicy: "On-Mesh Traffic (p) / 672",
}

export const OTBR_MOCK_ROUTERS: RouterEntry[] = [
  {
    id: "0xfc00",
    routerId: 36,
    rloc16: "0xfc00",
    extAddress: "f0:f5:bd:ff:fe:10:4b:24",
    linkQualityIn: 3,
    linkQualityOut: 3,
    age: 0,
  },
  {
    id: "0xfc01",
    routerId: 37,
    rloc16: "0xfc01",
    extAddress: "f0:f5:bd:ff:fe:10:4b:25",
    linkQualityIn: 2,
    linkQualityOut: 2,
    age: 452,
  },
  {
    id: "0xfc02",
    routerId: 38,
    rloc16: "0xfc02",
    extAddress: "f0:f5:bd:ff:fe:10:4b:26",
    linkQualityIn: 3,
    linkQualityOut: 2,
    age: 903,
  },
]

export const OTBR_MOCK_CHILDREN: ChildEntry[] = [
  {
    id: "0x1801",
    childId: 1,
    rloc16: "0x1801",
    extAddress: "a0:b0:c0:ff:fe:d0:e0:f0",
    linkQualityIn: 3,
    averageRssi: -45,
    fullThreadDevice: true,
    rxOnWhenIdle: true,
    age: 120,
  },
  {
    id: "0x1802",
    childId: 2,
    rloc16: "0x1802",
    extAddress: "a0:b0:c0:ff:fe:d0:e0:f1",
    linkQualityIn: 2,
    averageRssi: -61,
    fullThreadDevice: false,
    rxOnWhenIdle: false,
    age: 540,
  },
  {
    id: "0x1803",
    childId: 3,
    rloc16: "0x1803",
    extAddress: "a0:b0:c0:ff:fe:d0:e0:f2",
    linkQualityIn: 2,
    averageRssi: -58,
    fullThreadDevice: true,
    rxOnWhenIdle: true,
    age: 301,
  },
]

export const OTBR_MOCK_JOINERS: JoinerEntry[] = [
  {
    id: "eui64:f0f5bdfffe104b27",
    type: "eui64",
    sharedId: "f0:f5:bd:ff:fe:10:4b:27",
    pskd: "J01NME",
    expirationTime: 120_000,
  },
  {
    id: "any",
    type: "any",
    sharedId: "—",
    pskd: "88OPEN",
    expirationTime: 300_000,
  },
]
