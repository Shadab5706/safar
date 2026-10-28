import { latLngBounds, type LatLng as LeafletLatLng } from 'leaflet'
import { useEffect, useMemo, useState, type ReactNode } from 'react'
import { CircleMarker, MapContainer, Pane, Polyline, Popup, TileLayer, Tooltip, useMap } from 'react-leaflet'
import type { LatLng, MetroStation } from '../api/types'
import type { ClosuresApi } from '../hooks/useClosures'
import type { MetroNetwork } from '../hooks/useMetroNetwork'
import { CLOSED_COLOR, networkSegments, segmentClosureId, stationClosureId, type NetworkSegment } from '../lib/closures'
import type { Bounds } from '../lib/route'

const DELHI: LatLng = [28.6139, 77.209]

export interface StationAction {
  label: string
  onClick: () => void
}

interface Props {
  network: MetroNetwork | null
  /** Makes stations and line sections clickable to close or reopen them. */
  closures?: ClosuresApi
  /** Extra buttons in a station's popup ("Start here", "Add to trip"). */
  stationActions?: (station: MetroStation) => StationAction[]
  bounds: Bounds | null
  fitKey: string
  children?: ReactNode
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

type Selection =
  | { kind: 'station'; station: MetroStation }
  | { kind: 'segment'; segment: NetworkSegment; at: LeafletLatLng }

function PopupButton({ onClick, children, danger }: { onClick: () => void; children: ReactNode; danger?: boolean }) {
  return (
    <button
      type="button"
      onClick={onClick}
      className={`w-full rounded-md px-2.5 py-1.5 text-left text-xs font-semibold focus-visible:outline-2 focus-visible:outline-teal-700 ${
        danger ? 'bg-red-50 text-red-800 hover:bg-red-100' : 'bg-slate-100 text-slate-800 hover:bg-slate-200'
      }`}
    >
      {children}
    </button>
  )
}

function NetworkLayer({
  network,
  closures,
  stationActions,
}: {
  network: MetroNetwork
  closures?: ClosuresApi
  stationActions?: Props['stationActions']
}) {
  const map = useMap()
  const [selected, setSelected] = useState<Selection | null>(null)
  const stationById = useMemo(() => new Map(network.stations.map((s) => [s.id, s])), [network])
  const segments = useMemo(() => networkSegments(network.lines, stationById), [network, stationById])
  const lineNames = (s: MetroStation) => [...new Set(s.lines.map((id) => network.lineById.get(id)?.name ?? id))].join(', ')
  const interactive = Boolean(closures || stationActions)
  const closedStations = closures?.sets.stations
  const closedSegments = closures?.sets.segments

  function done(run: () => void) {
    run()
    setSelected(null)
    map.closePopup()
  }

  let popup: ReactNode = null
  if (selected?.kind === 'station') {
    const s = selected.station
    const closed = closedStations?.has(s.id) ?? false
    popup = (
      <Popup position={[s.lat, s.lng]} eventHandlers={{ remove: () => setSelected(null) }}>
        <div className="w-52 space-y-2">
          <div>
            <p className="text-sm font-bold text-slate-900">{s.name}</p>
            <p className="text-xs text-slate-600">{lineNames(s)}</p>
            {closed && <p className="mt-1 text-xs font-semibold text-red-800">Closed: trains run through without stopping</p>}
          </div>
          {stationActions?.(s).map((a) => (
            <PopupButton key={a.label} onClick={() => done(a.onClick)}>
              {a.label}
            </PopupButton>
          ))}
          {closures && (
            <PopupButton
              danger={!closed}
              onClick={() =>
                done(() => (closed ? closures.reopen(stationClosureId(s.id)) : closures.close({ type: 'station', station: s.id })))
              }
            >
              {closed ? 'Reopen station' : 'Close station'}
            </PopupButton>
          )}
        </div>
      </Popup>
    )
  } else if (selected?.kind === 'segment' && closures) {
    const { segment, at } = selected
    const closed = closedSegments?.has(segment.id) ?? false
    popup = (
      <Popup position={at} eventHandlers={{ remove: () => setSelected(null) }}>
        <div className="w-56 space-y-2">
          <div>
            <p className="text-sm font-bold text-slate-900">{segment.line.name}</p>
            <p className="text-xs text-slate-600">
              {segment.a.name} – {segment.b.name}
            </p>
            {closed && <p className="mt-1 text-xs font-semibold text-red-800">Closed: no trains on this section</p>}
          </div>
          <PopupButton
            danger={!closed}
            onClick={() =>
              done(() =>
                closed
                  ? closures.reopen(segmentClosureId(segment.line.id, segment.a.id, segment.b.id))
                  : closures.close({ type: 'segment', line: segment.line.id, from: segment.a.id, to: segment.b.id }),
              )
            }
          >
            {closed ? 'Reopen this section' : 'Close this section'}
          </PopupButton>
        </div>
      </Popup>
    )
  }

  return (
    <>
      <Pane name="network" style={{ zIndex: 405 }}>
        {segments.map((seg) => (
          <Polyline
            key={seg.id}
            positions={[
              [seg.a.lat, seg.a.lng],
              [seg.b.lat, seg.b.lng],
            ]}
            pathOptions={{ color: seg.line.color, weight: 3, opacity: 0.35, lineCap: 'round' }}
            interactive={false}
          />
        ))}
        {segments
          .filter((seg) => closedSegments?.has(seg.id))
          .map((seg) => (
            <Polyline
              key={`closed-${seg.id}`}
              positions={[
                [seg.a.lat, seg.a.lng],
                [seg.b.lat, seg.b.lng],
              ]}
              pathOptions={{ color: CLOSED_COLOR, weight: 5, opacity: 0.95, dashArray: '6 6' }}
              interactive={false}
            />
          ))}
        {closures &&
          segments.map((seg) => (
            <Polyline
              key={`hit-${seg.id}`}
              positions={[
                [seg.a.lat, seg.a.lng],
                [seg.b.lat, seg.b.lng],
              ]}
              pathOptions={{ color: '#000000', weight: 14, opacity: 0 }}
              eventHandlers={{ click: (e) => setSelected({ kind: 'segment', segment: seg, at: e.latlng }) }}
            >
              <Tooltip sticky>
                {seg.line.name}: {seg.a.name} – {seg.b.name}
              </Tooltip>
            </Polyline>
          ))}
        {network.stations.map((s) => {
          const closed = closedStations?.has(s.id) ?? false
          return (
            <CircleMarker
              key={`${s.id}:${closed ? 'closed' : 'open'}`}
              center={[s.lat, s.lng]}
              radius={closed ? 6 : interactive ? 4 : 2.5}
              bubblingMouseEvents={false}
              pathOptions={
                closed
                  ? { color: CLOSED_COLOR, weight: 3, opacity: 1, fillColor: '#fee2e2', fillOpacity: 1 }
                  : { color: '#475569', weight: 1, opacity: 0.5, fillColor: '#ffffff', fillOpacity: 0.9 }
              }
              eventHandlers={interactive ? { click: () => setSelected({ kind: 'station', station: s }) } : undefined}
            >
              <Tooltip direction="top" offset={[0, -3]}>
                {closed ? `${s.name} (closed)` : s.name}
              </Tooltip>
            </CircleMarker>
          )
        })}
      </Pane>
      {popup}
    </>
  )
}

/** The Delhi map with the metro network, shared by every screen. */
export function BaseMap({ network, closures, stationActions, bounds, fitKey, children }: Props) {
  return (
    <MapContainer center={DELHI} zoom={10} minZoom={8} maxZoom={18} zoomControl className="size-full" attributionControl>
      <TileLayer
        url="https://tile.openstreetmap.org/{z}/{x}/{y}.png"
        maxZoom={19}
        attribution='&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
      />
      {network && <NetworkLayer network={network} closures={closures} stationActions={stationActions} />}
      {children}
      <ViewController bounds={bounds} fitKey={fitKey} />
    </MapContainer>
  )
}

