import React from "react"
import { useTranslation } from "react-i18next"
import {
  NmxButton,
  NmxIconFontSymbol,
  NmxInlineAlert,
  NmxToolbar,
  NmxToolbarActions,
  NmxToolbarContent,
  NmxToolbarHeader,
  NmxToolbarList,
} from "@namorix/ui"
import type { NmxToolbarItemData } from "@namorix/ui"
import { useAppSelector } from "../../store/hooks"
import { selectNetworkError } from "../../store/selectors/networkSelectors"
import { useOtbrData } from "../../hooks/useOtbrData"
import { OtbrStatusPanel } from "./OtbrStatusPanel"
import { ThreadDatasetPanel } from "./ThreadDatasetPanel"
import { ThreadMeshPanel } from "./ThreadMeshPanel"
import { ThreadJoinerPanel } from "./ThreadJoinerPanel"

type NetworkTab = "overview" | "dataset" | "mesh" | "joiner"

const TABS: NmxToolbarItemData<NetworkTab>[] = [
  {
    key: "overview",
    icon: NmxIconFontSymbol.STATS,
    label: "network.tabs.overview",
  },
  {
    key: "dataset",
    icon: NmxIconFontSymbol.SECURITY,
    label: "network.tabs.dataset",
  },
  { key: "mesh", icon: NmxIconFontSymbol.NODES, label: "network.tabs.mesh" },
  {
    key: "joiner",
    icon: NmxIconFontSymbol.DEVICE,
    label: "network.tabs.joiner",
  },
]

export const NetworkView: React.FC = () => {
  const { t } = useTranslation()
  useOtbrData()

  const error = useAppSelector(selectNetworkError)

  if (error) {
    return <NmxInlineAlert semantic="error" message={error} />
  }

  return (
    <NmxToolbar<NetworkTab> defaultTab="overview">
      <NmxToolbarHeader>
        <NmxToolbarList items={TABS} t={t} />
      </NmxToolbarHeader>
      <NmxToolbarContent<NetworkTab> tabKey="overview">
        <OtbrStatusPanel />
      </NmxToolbarContent>
      <NmxToolbarContent<NetworkTab> tabKey="dataset">
        <ThreadDatasetPanel />
      </NmxToolbarContent>
      <NmxToolbarContent<NetworkTab> tabKey="mesh">
        <ThreadMeshPanel />
      </NmxToolbarContent>
      <NmxToolbarContent<NetworkTab> tabKey="joiner">
        <ThreadJoinerPanel />
      </NmxToolbarContent>
    </NmxToolbar>
  )
}
