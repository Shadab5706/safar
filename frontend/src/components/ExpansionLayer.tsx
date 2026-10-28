import { CircleMarker, Pane, Tooltip } from 'react-leaflet'
import type { Leg, MetroStation } from '../api/types'
import type { Frame } from '../lib/lab'
import { EndpointMarker, RouteLines } from './RouteLayer'

interface Props {
  frames: readonly Frame[]
  visible: number
  stationById: Map<string, MetroStation>
  color: string
  path: readonly Leg[] | null // drawn once the animation reaches the end
  from: MetroStation | undefined
  to: MetroStation | undefined
}

/** The stations a search has expanded so far, oldest first; the newest one is ringed. */
export function ExpansionLayer({ frames, visible, stationById, color, path, from, to }: Props) {
  const shown = frames.slice(0, visible)
  const last = shown.at(-1)
  const lastStation = last ? stationById.get(last.station) : undefined
  return (
    <>
      <Pane name="expansion" style={{ zIndex: 415 }}>
        {shown.map((f, i) => {
          const s = stationById.get(f.station)
          if (!s) return null
          return (
            <CircleMarker
              key={f.station}
              center={[s.lat, s.lng]}
              radius={5}
              pathOptions={{ color: '#ffffff', weight: 1, fillColor: color, fillOpacity: 0.8 }}
            >
              <Tooltip direction="top" offset={[0, -4]}>
                #{i + 1} {s.name}
              </Tooltip>
            </CircleMarker>
          )
        })}
        {lastStation && !path && (
          <CircleMarker
            center={[lastStation.lat, lastStation.lng]}
            radius={11}
            interactive={false}
            pathOptions={{ color, weight: 3, fill: false, opacity: 1 }}
          />
        )}
      </Pane>
      {path && (
        <Pane name="route" style={{ zIndex: 420 }}>
          <RouteLines legs={path} />
        </Pane>
      )}
      <Pane name="endpoints" style={{ zIndex: 430 }}>
        {from && <EndpointMarker point={from} kind="from" />}
        {to && <EndpointMarker point={to} kind="to" />}
      </Pane>
    </>
  )
}
