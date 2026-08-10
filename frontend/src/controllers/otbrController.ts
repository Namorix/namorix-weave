import type { AppDispatch } from "../store/store"
import { networkActions } from "../store/slices/networkSlice"
import {
  OTBR_MOCK_CHILDREN,
  OTBR_MOCK_DATASET,
  OTBR_MOCK_JOINERS,
  OTBR_MOCK_ROUTERS,
  OTBR_MOCK_STATUS,
} from "./otbrMockData"

export function startOtbrDataFeed(dispatch: AppDispatch): () => void {
  dispatch(networkActions.setLoading(true))
  dispatch(networkActions.setError(null))

  const timer = setTimeout(() => {
    dispatch(networkActions.setOtbrStatus(OTBR_MOCK_STATUS))
    dispatch(networkActions.setThreadDataset(OTBR_MOCK_DATASET))
    dispatch(networkActions.setRouterTable(OTBR_MOCK_ROUTERS))
    dispatch(networkActions.setChildTable(OTBR_MOCK_CHILDREN))
    dispatch(networkActions.setJoinerTable(OTBR_MOCK_JOINERS))
    dispatch(networkActions.setLoading(false))
  }, 700)

  return () => clearTimeout(timer)
}
