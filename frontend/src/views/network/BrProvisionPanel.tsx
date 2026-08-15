import React, { useState } from "react"
import { useTranslation } from "react-i18next"
import {
  NmxAlign,
  NmxBadge,
  NmxButton,
  NmxCard,
  NmxCardBody,
  NmxCardFooter,
  NmxDialog,
  NmxDialogBody,
  NmxDialogFooter,
  NmxDialogHeader,
  NmxForm,
  NmxFormActions,
  NmxFormField,
  NmxFormInput,
  NmxGrid,
  NmxIconFont,
  NmxIconFontSymbol,
  NmxInlineAlert,
  NmxMetaItem,
  NmxMetaList,
  NmxSpinner,
} from "@namorix/ui"
import type { NmxSemanticColor } from "@namorix/ui"
import { useNetworks } from "../../hooks/useNetworks"
import { useAppSelector, selectNetworks } from "../../store"
import { networkController } from "../../controllers/network"
import type { Network, NetworkStatus } from "../../types"

interface BrProvisionPanelProps {
  onSelect: (networkId: number) => void
}

const StatusSemantic: Record<NetworkStatus, NmxSemanticColor> = {
  Pending: "warning",
  Connected: "success",
  Offline: "default",
  Rejected: "error",
}

export const BrProvisionPanel: React.FC<BrProvisionPanelProps> = ({
  onSelect,
}) => {
  const { t } = useTranslation()
  const networks = useAppSelector(selectNetworks)
  const [acceptTarget, setAcceptTarget] = useState<Network | null>(null)
  const [rejectTarget, setRejectTarget] = useState<Network | null>(null)
  const [name, setName] = useState("")
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState<string | null>(null)

  useNetworks()

  const openAccept = (network: Network) => {
    setAcceptTarget(network)
    setName(network.name ?? "")
    setError(null)
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
    setError(null)
    try {
      await networkController.acceptNetwork(acceptTarget.id, {
        name: name || null,
        dataset: null,
      })
      setAcceptTarget(null)
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err))
    } finally {
      setBusy(false)
    }
  }

  const submitReject = async () => {
    if (!rejectTarget || busy) return
    setBusy(true)
    setError(null)
    try {
      await networkController.rejectNetwork(rejectTarget.id)
      setRejectTarget(null)
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err))
    } finally {
      setBusy(false)
    }
  }

  const formatFirstSeen = (value?: string) =>
    value ? new Date(value).toLocaleString() : "—"

  return (
    <>
      {networks.length === 0 ? (
        <NmxCard>
          <NmxCardBody>
            <NmxAlign direction="column" gap="sm" align="start">
              <NmxIconFont symbol={NmxIconFontSymbol.NETWORK} />
              <span>{t("network.list.empty")}</span>
            </NmxAlign>
          </NmxCardBody>
        </NmxCard>
      ) : (
        <NmxGrid cols={2} minColWidth={280} gap="md">
          {networks.map((network) => (
            <NmxCard key={network.id} onClick={() => onSelect(network.id)}>
              <NmxCardBody>
                <NmxAlign direction="row" justify="between" gap="sm">
                  <strong>
                    {network.name ?? network.eui64 ?? `#${network.id}`}
                  </strong>
                  <NmxBadge semantic={StatusSemantic[network.status]}>
                    {t(`network.statusLabel.${network.status}`)}
                  </NmxBadge>
                </NmxAlign>
                <NmxMetaList>
                  <NmxMetaItem
                    label={t("network.list.protocol")}
                    value={network.protocol}
                  />
                  <NmxMetaItem
                    label={t("network.list.host")}
                    value={network.host ?? "—"}
                  />
                  <NmxMetaItem
                    label={t("network.list.eui64")}
                    value={network.eui64 ?? "—"}
                  />
                  <NmxMetaItem
                    label={t("network.list.firstSeen")}
                    value={formatFirstSeen(network.firstSeenAt)}
                  />
                </NmxMetaList>
              </NmxCardBody>
              <NmxCardFooter>
                <NmxAlign direction="row" gap="sm">
                  {network.status === "Pending" && (
                    <>
                      <NmxButton
                        variant="outline"
                        semantic="error"
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
                        onClick={(e) => {
                          e.stopPropagation()
                          openAccept(network)
                        }}
                      >
                        <NmxIconFont symbol={NmxIconFontSymbol.CHECK} />
                        <span>{t("network.list.accept")}</span>
                      </NmxButton>
                    </>
                  )}
                  <NmxButton
                    variant="ghost"
                    onClick={(e) => {
                      e.stopPropagation()
                      onSelect(network.id)
                    }}
                  >
                    {t("network.list.open")}
                  </NmxButton>
                </NmxAlign>
              </NmxCardFooter>
            </NmxCard>
          ))}
        </NmxGrid>
      )}

      <NmxDialog open={acceptTarget !== null} onClose={closeAccept} size="sm">
        <NmxDialogHeader
          title={t("network.acceptDialog.title")}
          onClose={closeAccept}
        />
        <NmxDialogBody>
          {error && <NmxInlineAlert message={error} semantic="error" />}
          <NmxForm onSubmit={() => submitAccept()}>
            <NmxFormField label={t("network.acceptDialog.nameLabel")}>
              <NmxFormInput
                value={name}
                placeholder={t("network.acceptDialog.namePlaceholder")}
                onValueChange={setName}
                disabled={busy}
              />
            </NmxFormField>
            <NmxFormActions>
              <NmxButton
                variant="outline"
                onClick={closeAccept}
                disabled={busy}
              >
                {t("network.common.cancel")}
              </NmxButton>
              <NmxButton type="submit" semantic="success" disabled={busy}>
                {busy ? (
                  <NmxSpinner size="sm" />
                ) : (
                  <NmxIconFont symbol={NmxIconFontSymbol.CHECK} />
                )}
                <span>{t("network.acceptDialog.submit")}</span>
              </NmxButton>
            </NmxFormActions>
          </NmxForm>
        </NmxDialogBody>
      </NmxDialog>

      <NmxDialog open={rejectTarget !== null} onClose={closeReject} size="sm">
        <NmxDialogHeader
          title={t("network.rejectDialog.title")}
          onClose={closeReject}
        />
        <NmxDialogBody>
          {error && <NmxInlineAlert message={error} semantic="error" />}
          <span>{t("network.rejectDialog.body")}</span>
        </NmxDialogBody>
        <NmxDialogFooter>
          <NmxAlign direction="row" justify="end" gap="sm">
            <NmxButton variant="outline" onClick={closeReject} disabled={busy}>
              {t("network.common.cancel")}
            </NmxButton>
            <NmxButton semantic="danger" onClick={submitReject} disabled={busy}>
              {busy ? (
                <NmxSpinner size="sm" />
              ) : (
                <NmxIconFont symbol={NmxIconFontSymbol.CLOSE} />
              )}
              <span>{t("network.rejectDialog.confirm")}</span>
            </NmxButton>
          </NmxAlign>
        </NmxDialogFooter>
      </NmxDialog>
    </>
  )
}
