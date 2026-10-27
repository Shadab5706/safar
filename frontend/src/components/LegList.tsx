import { ArrowRightLeft, Crosshair } from 'lucide-react'
import type { Leg, Route } from '../api/types'
import { formatClock, formatDistance, formatDuration, formatRupees } from '../lib/format'
import { legColor, legStartOffsets, legTitle, textOn } from '../lib/route'
import { ModeIcon } from './ModeIcon'
import { ProviderQuotes } from './ProviderQuotes'

function LegDetail({ leg }: { leg: Leg }) {
  switch (leg.kind) {
    case 'metro':
      return (
        <>
          <p className="text-sm text-slate-800">
            {leg.from.name} → {leg.to.name}
          </p>
          <p className="text-xs text-slate-500">
            {leg.stops} {leg.stops === 1 ? 'stop' : 'stops'} · {formatDuration(leg.ride_min)} ride · ~
            {formatDuration(leg.wait_min)} wait · {formatDistance(leg.distance_km)}
            {leg.entry_min > 0 && ` · ${formatDuration(leg.entry_min)} to the platform`}
          </p>
        </>
      )
    case 'road':
      return (
        <>
          <p className="text-sm text-slate-800">
            {leg.from.name} → {leg.to.name}
          </p>
          <p className="text-xs text-slate-500">
            {formatDistance(leg.distance_km)} · {formatDuration(leg.ride_min)} ride · ~{formatDuration(leg.wait_min)}{' '}
            pickup · from <span className="font-medium text-slate-700">{formatRupees(leg.fare_rs)}</span>{' '}
            <span className="text-amber-800">est.</span>
          </p>
          <ProviderQuotes options={leg.options} mode={leg.mode} />
        </>
      )
    case 'walk':
      return (
        <p className="text-sm text-slate-800">
          {formatDistance(leg.distance_km)} to {leg.to.name}
        </p>
      )
    case 'transfer':
      return <p className="text-xs text-slate-500">Walk between platforms</p>
  }
}

function LegBadge({ leg }: { leg: Leg }) {
  const color = legColor(leg)
  if (leg.kind === 'metro') {
    return (
      <span className="rounded px-1.5 py-0.5 text-xs font-semibold" style={{ backgroundColor: color, color: textOn(color) }}>
        {leg.line.name}
      </span>
    )
  }
  return (
    <span className="inline-flex items-center gap-1 text-sm font-semibold text-slate-900">
      {leg.kind === 'road' && <ModeIcon mode={leg.mode} className="size-4" style={{ color }} />}
      {leg.kind === 'walk' && <ModeIcon mode="walk" className="size-4 text-slate-500" />}
      {leg.kind === 'transfer' && <ArrowRightLeft className="size-4 text-slate-500" aria-hidden="true" />}
      {legTitle(leg)}
    </span>
  )
}

interface Props {
  route: Route
  departAt: string
  onFocusLeg: (index: number) => void
}

export function LegList({ route, departAt, onFocusLeg }: Props) {
  const starts = legStartOffsets(route)
  return (
    <ol className="relative space-y-0" aria-label="Steps">
      {route.legs.map((leg, i) => {
        const color = legColor(leg)
        const dashed = leg.kind !== 'metro'
        return (
          <li key={i} className="relative flex gap-3 pb-3 last:pb-0">
            <div className="flex w-11 shrink-0 flex-col items-end pt-0.5">
              <time className="text-xs font-medium text-slate-600 tabular-nums">{formatClock(departAt, starts[i])}</time>
            </div>
            <div className="relative flex w-3 shrink-0 justify-center" aria-hidden="true">
              <span className="absolute top-1.5 size-3 rounded-full border-2 bg-white" style={{ borderColor: color }} />
              <span
                className="mt-5 w-1 rounded-full"
                style={
                  dashed
                    ? { backgroundImage: `repeating-linear-gradient(${color} 0 4px, transparent 4px 8px)` }
                    : { backgroundColor: color }
                }
              />
            </div>
            <div className="min-w-0 flex-1">
              <div className="flex items-center justify-between gap-2">
                <LegBadge leg={leg} />
                <span className="flex items-center gap-1">
                  <span className="text-xs font-medium text-slate-600 tabular-nums">{formatDuration(leg.total_min)}</span>
                  {leg.points.length >= 2 && (
                    <button
                      type="button"
                      onClick={() => onFocusLeg(i)}
                      aria-label={`Zoom the map to step ${i + 1}: ${legTitle(leg)}`}
                      title="Zoom map to this step"
                      className="rounded p-1 text-slate-400 hover:bg-slate-100 hover:text-teal-700 focus-visible:outline-2 focus-visible:outline-teal-600"
                    >
                      <Crosshair className="size-3.5" aria-hidden="true" />
                    </button>
                  )}
                </span>
              </div>
              <div className="mt-0.5">
                <LegDetail leg={leg} />
              </div>
            </div>
          </li>
        )
      })}
      <li className="flex gap-3">
        <div className="flex w-11 shrink-0 justify-end">
          <time className="text-xs font-semibold text-slate-900 tabular-nums">{formatClock(departAt, route.total_min)}</time>
        </div>
        <div className="flex w-3 shrink-0 justify-center" aria-hidden="true">
          <span className="mt-0.5 size-3 rounded-full bg-slate-900" />
        </div>
        <p className="text-sm font-semibold text-slate-900">Arrive at {route.legs.at(-1)?.to.name}</p>
      </li>
    </ol>
  )
}
