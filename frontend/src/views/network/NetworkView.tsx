import React, { useState } from "react"
import {
  NmxIconFontSymbol,
  NmxToolbar,
  NmxToolbarContent,
  NmxToolbarHeader,
} from "@namorix/ui"
import { useTranslation } from "react-i18next"
import { BrProvisionPanel } from "./BrProvisionPanel"
import { ThreadNetworkView } from "./ThreadNetworkView"

export const NetworkView: React.FC = () => {
  const { t } = useTranslation()
  const [selectedId, setSelectedId] = useState<number | null>(null)

  if (selectedId != null) {
    return (
      <ThreadNetworkView
        networkId={selectedId}
        onBack={() => setSelectedId(null)}
      />
    )
  }

  return (
    <NmxToolbar>
      <NmxToolbarHeader
        title={t("network.title")}
        icon={NmxIconFontSymbol.NETWORK}
        onBack={() => {}}
      />
      <NmxToolbarContent>
        <BrProvisionPanel onSelect={setSelectedId} />
      </NmxToolbarContent>
    </NmxToolbar>
  )
}
