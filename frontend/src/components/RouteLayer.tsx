import { CircleMarker, Pane, Polyline, Tooltip } from 'react-leaflet'
import type { Leg } from '../api/types'
import { formatDuration } from '../lib/format'
import { legStyle, legTitle } from '../lib/route'

/** The legs of one route: white casing, then each leg in its mode's style, plus change points. */
export function RouteLines({ legs, dim = false }: { legs: readonly Leg[]; dim?: boolean }) {
  const drawable = legs.filter((leg) => leg.points.length >= 2)
  return (
    <>
      {drawable.map((leg, i) => (
        <Polyline
          key={`casing-${i}`}
          positions={leg.points}
          pathOptions={{ color: '#ffffff', weight: legStyle(leg).weight + 4, opacity: dim ? 0.5 : 0.9, lineCap: 'round', lineJoin: 'round' }}
          interactive={false}
        />
      ))}
      {drawable.map((leg, i) => (
        <Polyline
          key={`leg-${i}`}
          positions={leg.points}
          pathOptions={{ ...legStyle(leg), opacity: dim ? 0.55 : 1, lineCap: 'round', lineJoin: 'round' }}
        >
          <Tooltip sticky>
            {legTitle(leg)} · {formatDuration(leg.total_min)}
          </Tooltip>
        </Polyline>
      ))}
      {legs.slice(1).map((leg, i) => (
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
    </>
  )
}

export function RouteLayer({ legs }: { legs: readonly Leg[] }) {
  return (
    <Pane name="route" style={{ zIndex: 420 }}>
      <RouteLines legs={legs} />
    </Pane>
  )
}

export function EndpointMarker({ point, kind }: { point: { lat: number; lng: number; name: string }; kind: 'from' | 'to' }) {
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
