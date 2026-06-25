import { createRoot } from "react-dom/client"
import type { AddonContext } from "@namorix/core"

export function mount(
  container: HTMLElement,
  context: AddonContext,
): () => void {
  const root = createRoot(container, {})

  root.render(<h1>Hello World — {context.addonId}</h1>)

  return () => root.unmount()
}
