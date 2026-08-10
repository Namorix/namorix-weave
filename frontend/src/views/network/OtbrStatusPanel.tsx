import React from "react"
import { useTranslation } from "react-i18next"
import {
  NmxBadge,
  NmxCard,
  NmxCardContainer,
  NmxCardSection,
  NmxMetaItem,
  NmxMetaList,
  type NmxSemanticColor,
} from "@namorix/ui"
import type { ThreadRole } from "../../types/network"
import { useAppSelector } from "../../store/hooks"
import {
  selectConnection,
  selectIpAddress,
  selectThreadRole,
  selectThreadVersion,
} from "../../store/selectors/networkSelectors"

const RoleKeys: Record<ThreadRole, string> = {
  disabled: "network.status.roleLabel.disabled",
  detached: "network.status.roleLabel.detached",
  child: "network.status.roleLabel.child",
  router: "network.status.roleLabel.router",
  leader: "network.status.roleLabel.leader",
}

const RoleSemantics: Record<ThreadRole, NmxSemanticColor> = {
  disabled: "trace",
  detached: "warning",
  child: "fatal",
  router: "info",
  leader: "success",
}

export const OtbrStatusPanel: React.FC = () => {
  const { t } = useTranslation()
  const connection = useAppSelector(selectConnection)
  const role = useAppSelector(selectThreadRole)
  const ipAddress = useAppSelector(selectIpAddress)
  const threadVersion = useAppSelector(selectThreadVersion)

  const hostPort =
    connection.host != null
      ? `${connection.host}:${connection.port ?? "—"}`
      : "—"

  return (
    <NmxCardContainer>
      <NmxCardSection title={t("network.status.title")}>
        <NmxCard spacing="xl">
          <NmxMetaList alignItem="end">
            <NmxMetaItem
              label={t("network.status.hostPort")}
              value={hostPort}
            />
            <NmxMetaItem label={t("network.status.status")}>
              <NmxBadge
                semantic={connection.isConnected ? "success" : "error"}
                size="sm"
              >
                {connection.isConnected
                  ? t("network.status.connected")
                  : t("network.status.disconnected")}
              </NmxBadge>
            </NmxMetaItem>
            <NmxMetaItem label={t("network.status.role")}>
              <NmxBadge semantic={RoleSemantics[role]} size="sm">
                {t(RoleKeys[role])}
              </NmxBadge>
            </NmxMetaItem>
            <NmxMetaItem
              label={t("network.status.ipAddress")}
              value={ipAddress ?? "—"}
            />
            <NmxMetaItem
              label={t("network.status.threadVersion")}
              value={threadVersion ?? "—"}
            />
          </NmxMetaList>
        </NmxCard>
      </NmxCardSection>
    </NmxCardContainer>
  )
}
