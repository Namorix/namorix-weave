import React, { useState } from "react"
import { useTranslation } from "react-i18next"
import {
  NmxButton,
  NmxCard,
  NmxCardBody,
  NmxCardContainer,
  NmxCardHeader,
  NmxCardSection,
  NmxMetaItem,
  NmxMetaList,
} from "@namorix/ui"
import { useAppSelector } from "../../store/hooks"
import { selectThreadDataset } from "../../store/selectors/networkSelectors"

const EMPTY = "—"
const NETWORK_KEY_MASK = "••••••••••••••••"

interface DatasetField {
  label: string
  value: string
}

export const ThreadDatasetPanel: React.FC = () => {
  const { t } = useTranslation()
  const dataset = useAppSelector(selectThreadDataset)
  const [networkKeyVisible, setNetworkKeyVisible] = useState(false)

  if (!dataset) {
    return (
      <NmxCardSection>
        <NmxCard>
          <NmxCardHeader title={t("network.dataset.title")} />
          <NmxCardBody>{t("network.dataset.empty")}</NmxCardBody>
        </NmxCard>
      </NmxCardSection>
    )
  }

  const items: DatasetField[] = [
    {
      label: t("network.dataset.networkName"),
      value: dataset.networkName ?? EMPTY,
    },
    { label: t("network.dataset.panId"), value: dataset.panId ?? EMPTY },
    {
      label: t("network.dataset.channel"),
      value: dataset.channel != null ? String(dataset.channel) : EMPTY,
    },
    {
      label: t("network.dataset.channelMask"),
      value: dataset.channelMask ?? EMPTY,
    },
    {
      label: t("network.dataset.extendedPanId"),
      value: dataset.extendedPanId ?? EMPTY,
    },
    {
      label: t("network.dataset.meshLocalPrefix"),
      value: dataset.meshLocalPrefix ?? EMPTY,
    },
    {
      label: t("network.dataset.networkKey"),
      value: networkKeyVisible
        ? (dataset.networkKey ?? EMPTY)
        : NETWORK_KEY_MASK,
    },
    { label: t("network.dataset.pskc"), value: dataset.pskc ?? EMPTY },
    {
      label: t("network.dataset.securityPolicy"),
      value: dataset.securityPolicy ?? EMPTY,
    },
    {
      label: t("network.dataset.activeTimestamp"),
      value: dataset.activeTimestamp ?? EMPTY,
    },
  ]

  return (
    <NmxCardContainer>
      <NmxCardSection title={t("network.dataset.title")}>
        <NmxCard spacing="xl">
          <NmxMetaList alignItem="end">
            {items.map((item, index) => (
              <NmxMetaItem key={index} label={item.label} value={item.value} />
            ))}
          </NmxMetaList>
        </NmxCard>
      </NmxCardSection>
      <NmxCardSection>
        <NmxButton
          uppercase={true}
          fullWidth={true}
          semantic={networkKeyVisible ? "success" : "warning"}
          onClick={() => setNetworkKeyVisible((visible) => !visible)}
        >
          {networkKeyVisible
            ? t("network.dataset.hideNetworkKey")
            : t("network.dataset.showNetworkKey")}
        </NmxButton>
      </NmxCardSection>
    </NmxCardContainer>
  )
}
