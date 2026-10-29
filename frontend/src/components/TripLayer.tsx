import { divIcon } from 'leaflet'
import { useMemo } from 'react'
import { Marker, Pane, Tooltip } from 'react-leaflet'
import type { Leg } from '../api/types'
import { RouteLines } from './RouteLayer'

export interface TripStop {
  key: string
  name: string
  lat: number
  lng: number
  label: string // what the marker shows: visiting position, or "S" for the start
  start: boolean
}

function stopIcon(label: string, start: boolean) {
  return divIcon({
    className: '',
    iconSize: [26, 26],
    iconAnchor: [13, 13],
    html: `<span class="flex size-[26px] items-center justify-center rounded-full border-2 border-surface text-xs font-bold text-on-brand shadow ${
      start ? 'bg-brand' : 'bg-ink'
    }">${label}</span>`,
  })
}

/** Numbered stop markers, plus every leg of the optimised tour once there is one. */
export function TripLayer({ stops, legs }: { stops: readonly TripStop[]; legs: readonly (readonly Leg[])[] }) {
  const icons = useMemo(() => stops.map((s) => stopIcon(s.label, s.start)), [stops])
  return (
    <>
      <Pane name="route" style={{ zIndex: 420 }}>
        {legs.map((l, i) => (
          <RouteLines key={i} legs={l} />
        ))}
      </Pane>
      <Pane name="trip-stops" style={{ zIndex: 640 }}>
        {stops.map((s, i) => (
          <Marker key={s.key} position={[s.lat, s.lng]} icon={icons[i]!} pane="trip-stops" keyboard={false}>
            <Tooltip direction="top" offset={[0, -12]}>
              {s.start ? 'Start' : `Stop ${s.label}`}: {s.name}
            </Tooltip>
          </Marker>
        ))}
      </Pane>
    </>
  )
}
