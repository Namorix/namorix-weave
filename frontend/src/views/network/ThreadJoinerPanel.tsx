import React, { useMemo } from "react"
import { useTranslation } from "react-i18next"
import {
  NmxAlign,
  NmxButton,
  NmxCard,
  NmxCardContainer,
  NmxCardSection,
  NmxDataTable,
  NmxIconFont,
  NmxIconFontSymbol,
} from "@namorix/ui"
import type { NmxDataTableColumn } from "@namorix/ui"
import type { JoinerEntry } from "../../types"
import { useAppSelector, selectJoinerEntries } from "../../store"

export const ThreadJoinerPanel: React.FC = () => {
  const { t } = useTranslation()
  const joiners = useAppSelector(selectJoinerEntries)

  const joinerColumns = useMemo<NmxDataTableColumn<JoinerEntry>[]>(
    () => [
      {
        header: t("network.tables.joiner.type"),
        renderCell: (row) => t(`network.tables.joiner.typeLabel.${row.type}`),
        alignCell: "center",
        alignHeader: "center",
      },
      {
        header: t("network.tables.joiner.sharedId"),
        renderCell: (row) => row.sharedId,
        grow: 1,
        alignCell: "center",
        alignHeader: "center",
      },
      {
        header: t("network.tables.joiner.pskd"),
        renderCell: (row) => row.pskd,
        grow: 1,
        alignCell: "center",
        alignHeader: "center",
      },
      {
        header: t("network.tables.joiner.expires"),
        renderCell: (row) =>
          row.expirationTime === 0
            ? t("network.tables.joiner.never")
            : t("network.tables.joiner.expiresIn", {
                seconds: Math.round(row.expirationTime / 1000),
              }),
        alignCell: "center",
        alignHeader: "center",
      },
    ],
    [t],
  )

  return (
    <NmxCardContainer>
      <NmxAlign direction="row" justify="end">
        <NmxButton>
          <NmxIconFont symbol={NmxIconFontSymbol.ADD} />
          <span>{t("network.joiner.add")}</span>
        </NmxButton>
      </NmxAlign>
      <NmxCardSection title={t("network.tables.joiner.title")}>
        <NmxCard spacing="none">
          <NmxDataTable
            columns={joinerColumns}
            rows={joiners}
            radiusEnabled={true}
            fallbackConditions={[
              {
                condition: joiners.length === 0,
                state: "empty",
                content: t("network.tables.empty"),
              },
            ]}
          />
        </NmxCard>
      </NmxCardSection>
    </NmxCardContainer>
  )
}
