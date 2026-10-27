import { latLngBounds } from 'leaflet'
import { useEffect, useMemo } from 'react'
import { CircleMarker, MapContainer, Pane, Polyline, TileLayer, Tooltip, useMap } from 'react-leaflet'
import type { LatLng, Route, SearchResult } from '../api/types'
import type { MetroNetwork } from '../hooks/useMetroNetwork'
import { formatDuration } from '../lib/format'
import { drawableLegs, legStyle, legTitle, routeBounds, type Bounds } from '../lib/route'

const DELHI: LatLng = [28.6139, 77.209]

interface Props {
  network: MetroNetwork | null
  route: Route | null
  focusLeg: number | null
  from: SearchResult | null
  to: SearchResult | null
}

/** Re-fit the view whenever `fitKey` changes, and keep Leaflet's size in sync with its box. */
function ViewController({ bounds, fitKey }: { bounds: Bounds | null; fitKey: string }) {
  const map = useMap()
  useEffect(() => {
    const el = map.getContainer()
    const ro = new ResizeObserver(() => map.invalidateSize())
    ro.observe(el)
    return () => ro.disconnect()
  }, [map])
  useEffect(() => {
    if (!bounds) return
    const b = latLngBounds(bounds)
    if (b.getNorthEast().equals(b.getSouthWest())) map.setView(b.getCenter(), 14)
    else map.fitBounds(b, { padding: [36, 36], maxZoom: 15 })
    // bounds is derived from fitKey; refitting on every render would fight the user's panning.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [map, fitKey])
  return null
}

function NetworkLayer({ network }: { network: MetroNetwork }) {
  const paths = useMemo(() => {
    const byId = new Map(network.stations.map((s) => [s.id, s]))
    return network.lines.map((line) => ({
      line,
      positions: line.stations.flatMap((id): LatLng[] => {
        const s = byId.get(id)
        return s ? [[s.lat, s.lng]] : []
      }),
    }))
  }, [network])

  return (
    <Pane name="network" style={{ zIndex: 405 }}>
      {paths.map(({ line, positions }) => (
        <Polyline
          key={line.id}
          positions={positions}
          pathOptions={{ color: line.color, weight: 3, opacity: 0.35 }}
          interactive={false}
        />
      ))}
      {network.stations.map((s) => (
        <CircleMarker
          key={s.id}
          center={[s.lat, s.lng]}
          radius={2.5}
          pathOptions={{ color: '#475569', weight: 1, opacity: 0.4, fillColor: '#ffffff', fillOpacity: 0.8 }}
        >
          <Tooltip direction="top" offset={[0, -3]}>
            {s.name}
          </Tooltip>
        </CircleMarker>
      ))}
    </Pane>
  )
}

function RouteLayer({ route }: { route: Route }) {
  const legs = drawableLegs(route)
  return (
    <Pane name="route" style={{ zIndex: 420 }}>
      {legs.map((leg, i) => {
        const style = legStyle(leg)
        return (
          <Polyline
            key={`casing-${i}`}
            positions={leg.points}
            pathOptions={{ color: '#ffffff', weight: style.weight + 4, opacity: 0.9, lineCap: 'round', lineJoin: 'round' }}
            interactive={false}
          />
        )
      })}
      {legs.map((leg, i) => {
        const style = legStyle(leg)
        return (
          <Polyline
            key={`leg-${i}`}
            positions={leg.points}
            pathOptions={{ ...style, opacity: 1, lineCap: 'round', lineJoin: 'round' }}
          >
            <Tooltip sticky>
              {legTitle(leg)} · {formatDuration(leg.total_min)}
            </Tooltip>
          </Polyline>
        )
      })}
      {route.legs.slice(1).map((leg, i) => (
        <CircleMarker
          key={`stop-${i}`}
          center={[leg.from.lat, leg.from.lng]}
          radius={5}
          pathOptions={{ color: '#0f172a', weight: 2, fillColor: '#ffffff', fillOpacity: 1 }}
        >
          <Tooltip direction="top" offset={[0, -5]}>
            {leg.from.name}
          </Tooltip>
        </CircleMarker>
      ))}
    </Pane>
  )
}

function EndpointMarker({ point, kind }: { point: { lat: number; lng: number; name: string }; kind: 'from' | 'to' }) {
  return (
    <CircleMarker
      center={[point.lat, point.lng]}
      radius={8}
      pathOptions={{ color: '#ffffff', weight: 3, fillColor: kind === 'from' ? '#0f766e' : '#be123c', fillOpacity: 1 }}
    >
      <Tooltip direction="top" offset={[0, -8]}>
        {kind === 'from' ? 'Start' : 'Destination'}: {point.name}
      </Tooltip>
    </CircleMarker>
  )
}

export function MapView({ network, route, focusLeg, from, to }: Props) {
  const focused = route && focusLeg !== null ? route.legs[focusLeg] : undefined

  let bounds: Bounds | null = null
  let fitKey = 'none'
  if (focused && focused.points.length > 0) {
    const lats = focused.points.map((p) => p[0])
    const lngs = focused.points.map((p) => p[1])
    bounds = [
      [Math.min(...lats), Math.min(...lngs)],
      [Math.max(...lats), Math.max(...lngs)],
    ]
    fitKey = `leg:${route?.summary}:${route?.rank}:${focusLeg}`
  } else if (route) {
    bounds = routeBounds(route)
    fitKey = `route:${route.summary}:${route.rank}:${route.total_min}`
  } else if (from || to) {
    const pts = [from, to].filter((p): p is SearchResult => p !== null)
    bounds = [
      [Math.min(...pts.map((p) => p.lat)), Math.min(...pts.map((p) => p.lng))],
      [Math.max(...pts.map((p) => p.lat)), Math.max(...pts.map((p) => p.lng))],
    ]
    fitKey = `ends:${from?.id}:${to?.id}`
  }

  const start = route?.legs[0]?.from ?? from
  const end = route?.legs.at(-1)?.to ?? to

  return (
    <MapContainer
      center={DELHI}
      zoom={10}
      minZoom={8}
      maxZoom={18}
      zoomControl
      className="size-full"
      attributionControl
    >
      <TileLayer
        url="https://tile.openstreetmap.org/{z}/{x}/{y}.png"
        maxZoom={19}
        attribution='&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
      />
      {network && <NetworkLayer network={network} />}
      {route && <RouteLayer key={`${route.rank}:${route.summary}`} route={route} />}
      <Pane name="endpoints" style={{ zIndex: 430 }}>
        {start && <EndpointMarker point={start} kind="from" />}
        {end && <EndpointMarker point={end} kind="to" />}
      </Pane>
      <ViewController bounds={bounds} fitKey={fitKey} />
    </MapContainer>
  )
}
