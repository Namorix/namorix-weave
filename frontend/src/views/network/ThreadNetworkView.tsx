import React from "react";
import {
  NmxIconFontSymbol,
  NmxToolbar,
  NmxToolbarContent,
  NmxToolbarHeader,
  type NmxToolbarItemData,
  NmxToolbarList,
} from "@namorix/ui"
import {OtbrStatusPanel} from "./OtbrStatusPanel"
import {ThreadDatasetPanel} from "./ThreadDatasetPanel"
import {ThreadMeshPanel} from "./ThreadMeshPanel"
import {ThreadJoinerPanel} from "./ThreadJoinerPanel"
import {useTranslation} from "react-i18next";
import {useOtbrData} from "../../hooks/useOtbrData"

type ThreadNetworkTab = "overview" | "dataset" | "mesh" | "joiner"

const TABS: NmxToolbarItemData<ThreadNetworkTab>[] = [
    {
        key: "overview",
        icon: NmxIconFontSymbol.STATS,
        label: "network.br.tabs.overview",
    },
    {
        key: "dataset",
        icon: NmxIconFontSymbol.SECURITY,
        label: "network.br.tabs.dataset",
    },
    { key: "mesh", icon: NmxIconFontSymbol.NODES, label: "network.br.tabs.mesh" },
    {
        key: "joiner",
        icon: NmxIconFontSymbol.DEVICE,
        label: "network.br.tabs.joiner",
    },
]

interface ThreadNetworkViewProps {
  networkId: number
  onBack: () => void
}

export const ThreadNetworkView: React.FC<ThreadNetworkViewProps> = ({
  networkId,
  onBack,
}) => {
    const { t } = useTranslation()

    useOtbrData(networkId)

    return <NmxToolbar<ThreadNetworkTab> defaultTab="overview">
        <NmxToolbarHeader
            title={t("network.title")}
            icon={NmxIconFontSymbol.NETWORK}
            onBack={onBack}
        >
            <NmxToolbarList items={TABS} t={t} />
        </NmxToolbarHeader>
        <NmxToolbarContent<ThreadNetworkTab> tabKey="overview">
            <OtbrStatusPanel />
        </NmxToolbarContent>
        <NmxToolbarContent<ThreadNetworkTab> tabKey="dataset">
            <ThreadDatasetPanel />
        </NmxToolbarContent>
        <NmxToolbarContent<ThreadNetworkTab> tabKey="mesh">
            <ThreadMeshPanel />
        </NmxToolbarContent>
        <NmxToolbarContent<ThreadNetworkTab> tabKey="joiner">
            <ThreadJoinerPanel />
        </NmxToolbarContent>
    </NmxToolbar>
}