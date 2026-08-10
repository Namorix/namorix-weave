import React, { useMemo } from "react"
import { useTranslation } from "react-i18next"
import {
  NmxCard,
  NmxCardContainer,
  NmxCardSection,
  NmxDataTable,
} from "@namorix/ui"
import type { NmxDataTableColumn } from "@namorix/ui"
import type { ChildEntry, RouterEntry } from "../../types/network"
import { useAppSelector } from "../../store/hooks"
import {
  selectChildEntries,
  selectRouterEntries,
} from "../../store/selectors/networkSelectors"

export const ThreadMeshPanel: React.FC = () => {
  const { t } = useTranslation()
  const routers = useAppSelector(selectRouterEntries)
  const children = useAppSelector(selectChildEntries)

  const routerColumns = useMemo<NmxDataTableColumn<RouterEntry>[]>(
    () => [
      {
        header: t("network.tables.router.routerId"),
        renderCell: (row) => String(row.routerId),
        alignCell: "center",
        alignHeader: "center",
      },
      {
        header: t("network.tables.router.rloc16"),
        renderCell: (row) => row.rloc16,
        grow: 1,
        alignCell: "center",
        alignHeader: "center",
      },
      {
        header: t("network.tables.router.extAddress"),
        renderCell: (row) => row.extAddress,
        grow: 1,
        hideBelow: "sm",
      },
      {
        header: t("network.tables.router.lqi"),
        renderCell: (row) => String(row.linkQualityIn),
      },
      {
        header: t("network.tables.router.lqo"),
        renderCell: (row) => String(row.linkQualityOut),
      },
      {
        header: t("network.tables.router.age"),
        renderCell: (row) => String(row.age),
      },
    ],
    [t],
  )

  const childColumns = useMemo<NmxDataTableColumn<ChildEntry>[]>(
    () => [
      {
        header: t("network.tables.child.childId"),
        renderCell: (row) => String(row.childId),
        alignCell: "center",
        alignHeader: "center",
      },
      {
        header: t("network.tables.child.rloc16"),
        renderCell: (row) => row.rloc16,
        grow: 1,
        alignCell: "center",
        alignHeader: "center",
      },
      {
        header: t("network.tables.child.extAddress"),
        renderCell: (row) => row.extAddress,
        grow: 1,
        hideBelow: "md",
        alignCell: "center",
        alignHeader: "center",
      },
      {
        header: t("network.tables.child.lqi"),
        renderCell: (row) => String(row.linkQualityIn),
        alignCell: "center",
        alignHeader: "center",
      },
      {
        header: t("network.tables.child.avgRssi"),
        renderCell: (row) => `${row.averageRssi} dBm`,
        alignCell: "center",
        alignHeader: "center",
      },
      {
        header: t("network.tables.child.ftd"),
        renderCell: (row) => (row.fullThreadDevice ? "FTD" : "MTD"),
        alignCell: "center",
        alignHeader: "center",
      },
      {
        header: t("network.tables.child.rxOnWhenIdle"),
        renderCell: (row) => (row.rxOnWhenIdle ? "On" : "Off"),
        hideBelow: "md",
        alignCell: "center",
        alignHeader: "center",
      },
      {
        header: t("network.tables.child.age"),
        renderCell: (row) => String(row.age),
        alignCell: "center",
        alignHeader: "center",
      },
    ],
    [t],
  )

  return (
    <NmxCardContainer>
      <NmxCardSection title={t("network.tables.router.title")}>
        <NmxCard spacing="none">
          <NmxDataTable
            columns={routerColumns}
            rows={routers}
            radiusEnabled={true}
            fallbackConditions={[
              {
                condition: routers.length === 0,
                state: "empty",
                content: t("network.tables.empty"),
              },
            ]}
          />
        </NmxCard>
      </NmxCardSection>

      <NmxCardSection title={t("network.tables.child.title")}>
        <NmxCard spacing="none">
          <NmxDataTable
            columns={childColumns}
            rows={children}
            radiusEnabled={true}
            fallbackConditions={[
              {
                condition: children.length === 0,
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
