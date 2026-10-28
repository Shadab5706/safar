import { Pane } from 'react-leaflet'
import type { Route, SearchResult } from '../api/types'
import type { ClosuresApi } from '../hooks/useClosures'
import type { MetroNetwork } from '../hooks/useMetroNetwork'
import { boundsOf, routeBounds, type Bounds } from '../lib/route'
import { BaseMap } from './BaseMap'
import { EndpointMarker, RouteLayer } from './RouteLayer'

interface Props {
  network: MetroNetwork | null
  route: Route | null
  focusLeg: number | null
  from: SearchResult | null
  to: SearchResult | null
  closures?: ClosuresApi
}

export function MapView({ network, route, focusLeg, from, to, closures }: Props) {
  const focused = route && focusLeg !== null ? route.legs[focusLeg] : undefined

  let bounds: Bounds | null = null
  let fitKey = 'none'
  if (focused && focused.points.length > 0) {
    bounds = boundsOf(focused.points.map(([lat, lng]) => ({ lat, lng })))
    fitKey = `leg:${route?.summary}:${route?.rank}:${focusLeg}`
  } else if (route) {
    bounds = routeBounds(route)
    fitKey = `route:${route.summary}:${route.rank}:${route.total_min}`
  } else if (from || to) {
    bounds = boundsOf([from, to].filter((p): p is SearchResult => p !== null))
    fitKey = `ends:${from?.id}:${to?.id}`
  }

  const start = route?.legs[0]?.from ?? from
  const end = route?.legs.at(-1)?.to ?? to

  return (
    <BaseMap network={network} closures={closures} bounds={bounds} fitKey={fitKey}>
      {route && <RouteLayer key={`${route.rank}:${route.summary}:${route.total_min}`} legs={route.legs} />}
      <Pane name="endpoints" style={{ zIndex: 430 }}>
        {start && <EndpointMarker point={start} kind="from" />}
        {end && <EndpointMarker point={end} kind="to" />}
      </Pane>
    </BaseMap>
  )
}
