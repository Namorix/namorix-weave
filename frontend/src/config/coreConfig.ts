import {
  createNmxCore,
  createAuthRefresh,
  createHttpClient,
  createSignalrService,
  createSignalRHooks,
} from "@namorix/core"

const config = createNmxCore({
  apiBaseUrl: import.meta.env.VITE_API_URL ?? window.location.origin,
  hubsPath: "/hubs/weave",
  isShellDesktop: false,
})

const authRefresh = createAuthRefresh(config)
const http = createHttpClient(authRefresh)
const signalr = createSignalrService({ core: config, authRefresh })
const signalRHooks = createSignalRHooks(signalr)

export const coreConfig = {
  ...config,
  http,
  authRefresh,
  signalr,
  signalRHooks,
}
