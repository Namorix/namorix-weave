import React, { useState } from "react"
import { formatCustomError, nmxToast } from "@namorix/core"
import {
  NmxAlign,
  NmxAlertDialog,
  NmxBadge,
  NmxButton,
  NmxCard,
  NmxCardBody,
  NmxCardFooter,
  NmxFormField,
  NmxFormInput,
  NmxGrid,
  NmxIconFont,
  NmxIconFontSymbol,
  NmxMetaItem,
  NmxMetaList,
  NmxToolbar,
  NmxToolbarContent,
  NmxToolbarHeader,
  type NmxSemanticColor,
  NmxCardHeader,
  NmxToolbarContainer,
  NmxButtonRefresh,
} from "@namorix/ui"
import { useTranslation } from "react-i18next"
import { ThreadNetworkView } from "./ThreadNetworkView"
import { useNetworks } from "../../hooks/useNetworks"
import { useAppSelector, selectNetworks } from "../../store"
import { networkController } from "../../controllers/network.controller"
import { type Network, type NetworkStatus } from "../../types"
import { NetworkErrorCodes } from "./NetworkErrorCodes.ts"

const StatusSemantic: Record<NetworkStatus, NmxSemanticColor> = {
  pending: "warning",
  connected: "success",
  offline: "default",
  rejected: "error",
}

export const NetworkView: React.FC = () => {
  const { t } = useTranslation()
  const networks = useAppSelector(selectNetworks)
  const [selectedId, setSelectedId] = useState<number | null>(null)
  const [acceptTarget, setAcceptTarget] = useState<Network | null>(null)
  const [rejectTarget, setRejectTarget] = useState<Network | null>(null)
  const [name, setName] = useState("")
  const [busy, setBusy] = useState(false)

  useNetworks()

  if (selectedId != null) {
    return (
      <ThreadNetworkView
        networkId={selectedId}
        onBack={() => setSelectedId(null)}
      />
    )
  }

  const openAccept = (network: Network) => {
    setAcceptTarget(network)
    setName(network.name ?? "")
  }

  const closeAccept = () => {
    if (!busy) setAcceptTarget(null)
  }

  const closeReject = () => {
    if (!busy) setRejectTarget(null)
  }

  const submitAccept = async () => {
    if (!acceptTarget || busy) return
    setBusy(true)
    networkController
      .acceptNetwork(acceptTarget.id, {
        name: name || null,
        dataset: null,
      })
      .then(() => {
        setAcceptTarget(null)
        nmxToast.success(t("network.acceptDialog.success"))
      })
      .catch((err) =>
        nmxToast.error(formatCustomError(t, err, NetworkErrorCodes)),
      )
      .finally(() => setBusy(false))
  }

  const submitReject = async () => {
    if (!rejectTarget || busy) return
    setBusy(true)
    networkController
      .rejectNetwork(rejectTarget.id)
      .then(() => {
        setRejectTarget(null)
        nmxToast.success(t("network.rejectDialog.success"))
      })
      .catch((err) =>
        nmxToast.error(formatCustomError(t, err, NetworkErrorCodes)),
      )
      .finally(() => setBusy(false))
  }

  const formatFirstSeen = (value?: string) =>
    value ? new Date(value).toLocaleString() : "—"

  return (
    <NmxToolbar>
      <NmxToolbarHeader
        title={t("network.title")}
        icon={NmxIconFontSymbol.NETWORK}
        onBack={() => {}}
      />
      <NmxToolbarContainer>
        <NmxToolbarContent spacing="md">
          <NmxAlign direction="row" justify="end">
            <NmxButton semantic="success">
              <NmxIconFont symbol={NmxIconFontSymbol.ADD} />
              <span>{t("network.addDevice")}</span>
            </NmxButton>
            <NmxButtonRefresh title={t("network.refresh")} />
          </NmxAlign>
          {networks.length === 0 ? (
            <NmxCard>
              <NmxCardBody isEmpty={true}>
                <span>{t("network.list.empty")}</span>
              </NmxCardBody>
            </NmxCard>
          ) : (
            <NmxGrid cols={2} minColWidth={280} gap="md">
              {networks.map((network) => (
                <NmxCard
                  key={network.id}
                  onClick={() => setSelectedId(network.id)}
                  spacing="lg"
                >
                  <NmxCardHeader
                    title={t(
                      "network.list.protocols." +
                        (network.protocol ?? "unknown"),
                    )}
                    spacing="md"
                  ></NmxCardHeader>
                  <NmxCardBody>
                    <NmxMetaList>
                      <NmxMetaItem
                        label={t("network.list.status")}
                        alignValue="end"
                      >
                        <NmxBadge
                          semantic={StatusSemantic[network.status]}
                          size="sm"
                        >
                          {t(`network.statusLabel.${network.status}`)}
                        </NmxBadge>
                      </NmxMetaItem>
                      <NmxMetaItem
                        label={t("network.list.host")}
                        value={network.host ?? "—"}
                        alignValue="end"
                      />
                      <NmxMetaItem
                        label={t("network.list.eui64")}
                        value={network.eui64 ?? "—"}
                        alignValue="end"
                      />
                      <NmxMetaItem
                        label={t("network.list.firstSeen")}
                        value={formatFirstSeen(network.firstSeenAt)}
                        alignValue="end"
                      />
                    </NmxMetaList>
                  </NmxCardBody>
                  {network.status === "pending" && (
                    <NmxCardFooter spacingBottom="xl">
                      <NmxAlign direction="row" justify="center">
                        <NmxButton
                          variant="outline"
                          semantic="error"
                          fullWidth={true}
                          uppercase={true}
                          onClick={(e) => {
                            e.stopPropagation()
                            setRejectTarget(network)
                          }}
                        >
                          <NmxIconFont symbol={NmxIconFontSymbol.CLOSE} />
                          <span>{t("network.list.reject")}</span>
                        </NmxButton>
                        <NmxButton
                          semantic="success"
                          fullWidth={true}
                          uppercase={true}
                          onClick={(e) => {
                            e.stopPropagation()
                            openAccept(network)
                          }}
                        >
                          <NmxIconFont symbol={NmxIconFontSymbol.CHECK} />
                          <span>{t("network.list.accept")}</span>
                        </NmxButton>
                      </NmxAlign>
                    </NmxCardFooter>
                  )}
                </NmxCard>
              ))}
            </NmxGrid>
          )}

          <NmxAlertDialog
            open={acceptTarget !== null}
            title={t("network.acceptDialog.title")}
            size="sm"
            confirmLabel={t("network.acceptDialog.submit")}
            confirmSemantic="success"
            closeLabel={t("network.common.cancel")}
            onClose={closeAccept}
            onConfirm={submitAccept}
            loading={busy}
          >
            <NmxFormField label={t("network.acceptDialog.nameLabel")}>
              <NmxFormInput
                value={name}
                placeholder={t("network.acceptDialog.namePlaceholder")}
                onValueChange={setName}
                disabled={busy}
              />
            </NmxFormField>
          </NmxAlertDialog>

          <NmxAlertDialog
            open={rejectTarget !== null}
            title={t("network.rejectDialog.title")}
            size="sm"
            confirmLabel={t("network.rejectDialog.confirm")}
            confirmSemantic="error"
            closeLabel={t("network.common.cancel")}
            onClose={closeReject}
            onConfirm={submitReject}
            loading={busy}
          >
            <p>{t("network.rejectDialog.body")}</p>
          </NmxAlertDialog>
        </NmxToolbarContent>
      </NmxToolbarContainer>
    </NmxToolbar>
  )
}
