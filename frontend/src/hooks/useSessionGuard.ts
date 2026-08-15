import { useEffect } from "react"
import { useIsStandalone } from "@namorix/core"

export function useSessionGuard(): void {
  const isStandalone = useIsStandalone()

  useEffect(() => {
    if (!isStandalone) return

    fetch("/api/oauth/status")
      .then((res) => {
        if (res.status === 401) window.location.replace("/api/oauth/login")
      })
      .catch(() => {
        // Desktop unreachable — keep the app mounted; error surfaces elsewhere.
      })
  }, [isStandalone])
}
