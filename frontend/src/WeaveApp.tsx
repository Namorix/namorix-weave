import React from "react"
import { Provider } from "react-redux"
import { useTranslation } from "react-i18next"
import {
  NmxRail,
  NmxRailList,
  NmxRailContent,
  NmxIconFontSymbol,
  NmxLoadingOverlay,
} from "@namorix/ui"
import type { NmxRailItemData } from "@namorix/ui"
import "./i18n"
import { useAddonMode, useIsStandalone, useSessionGuard } from "@namorix/core"
import { store } from "./store/store"
import { NetworkView } from "./views/network/NetworkView"

type WeaveTab = "dashboard" | "devices" | "network" | "settings"

const TABS: NmxRailItemData<WeaveTab>[] = [
  { key: "dashboard", icon: NmxIconFontSymbol.HOME, label: "Dashboard" },
  { key: "devices", icon: NmxIconFontSymbol.DEVICE, label: "Devices" },
  { key: "network", icon: NmxIconFontSymbol.NETWORK, label: "Network" },
  { key: "settings", icon: NmxIconFontSymbol.SETTING, label: "Settings" },
]

export const WeaveApp: React.FC = () => {
  const { t } = useTranslation()
  const isStandalone = useIsStandalone()
  const guard = useSessionGuard()
  const addonMode = useAddonMode()

  if (guard === "loading") return <NmxLoadingOverlay />
  if (guard === "unauthorized") return null

  return (
    <Provider store={store}>
      <NmxRail<WeaveTab> defaultTab="network">
        <NmxRailList
          title={t("weave.title")}
          items={TABS}
          t={t}
          showToggle={isStandalone}
        />
        <NmxRailContent<WeaveTab> tabKey="dashboard">
          <h1>DashboardView: {addonMode}</h1>
        </NmxRailContent>
        <NmxRailContent<WeaveTab> tabKey="devices">
          <h1>DevicesView</h1>
        </NmxRailContent>
        <NmxRailContent<WeaveTab>
          tabKey="network"
          spacingVerticalDisabled={true}
          spacingHorizontalDisabled={true}
        >
          <NetworkView />
        </NmxRailContent>
        <NmxRailContent<WeaveTab> tabKey="settings">
          <h1>SettingsView</h1>
        </NmxRailContent>
      </NmxRail>
    </Provider>
  )
}
