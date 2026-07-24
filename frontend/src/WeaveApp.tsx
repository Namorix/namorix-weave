import React from "react"
import { useTranslation } from "react-i18next"
import {
  NmxRail,
  NmxRailList,
  NmxRailContent,
  NmxIconFontSymbol,
} from "@namorix/ui"
import type { NmxRailItemData } from "@namorix/ui"
import "./i18n"
import {
  useAddonMode,
  useIsStandalone,
} from "../../../namorix/frontend/packages/core/src/host.ts"

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

  return (
    <NmxRail<WeaveTab> defaultTab="dashboard">
      <NmxRailList
        title={t("weave.title")}
        items={TABS}
        t={t}
        showToggle={isStandalone}
      />
      <NmxRailContent<WeaveTab> tabKey="dashboard">
        <h1>DashboardView: {useAddonMode()}</h1>
      </NmxRailContent>
      <NmxRailContent<WeaveTab> tabKey="devices">
        <h1>DevicesView</h1>
      </NmxRailContent>
      <NmxRailContent<WeaveTab> tabKey="network">
        <h1>NetworkView</h1>
      </NmxRailContent>
      <NmxRailContent<WeaveTab> tabKey="settings">
        <h1>SettingsView</h1>
      </NmxRailContent>
    </NmxRail>
  )
}
