import { createRoot } from "react-dom/client"
import {WeaveApp} from "./App.tsx";

export function mount(
  container: HTMLElement,
): () => void {
  const root = createRoot(container, {})
  root.render(<WeaveApp/>)
  return () => root.unmount()
}
