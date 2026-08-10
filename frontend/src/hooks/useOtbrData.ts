import { useEffect } from "react"
import { useAppDispatch } from "../store/hooks"
import { startOtbrDataFeed } from "../controllers/otbrController"

export function useOtbrData(): void {
  const dispatch = useAppDispatch()

  useEffect(() => startOtbrDataFeed(dispatch), [dispatch])
}
