import { useEffect, useState } from 'react'
import { getMetroLines, getMetroStations, isAbortError } from '../api/client'
import type { MetroLine, MetroStation } from '../api/types'

export interface MetroNetwork {
  lines: MetroLine[]
  stations: MetroStation[]
  lineById: Map<string, MetroLine>
}

export type NetworkState =
  | { status: 'loading' }
  | { status: 'ready'; network: MetroNetwork }
  | { status: 'error'; message: string }

/** Every metro line and station, loaded once for the map and the autocomplete. */
export function useMetroNetwork(): NetworkState {
  const [state, setState] = useState<NetworkState>({ status: 'loading' })
  useEffect(() => {
    const ctl = new AbortController()
    Promise.all([getMetroLines(ctl.signal), getMetroStations(ctl.signal)])
      .then(([lines, stations]) => {
        setState({ status: 'ready', network: { lines, stations, lineById: new Map(lines.map((l) => [l.id, l])) } })
      })
      .catch((err: unknown) => {
        if (isAbortError(err)) return
        setState({ status: 'error', message: err instanceof Error ? err.message : 'Could not load the metro map.' })
      })
    return () => ctl.abort()
  }, [])
  return state
}
