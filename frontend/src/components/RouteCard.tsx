import { ChevronDown } from 'lucide-react'
import type { Route, RouteTag } from '../api/types'
import { formatChanges, formatClock, formatDistance, formatDuration, formatRupees, tagLabel } from '../lib/format'
import { fareBreakdown, hasEstimatedFares, isDirectRide, routeChain, textOn } from '../lib/route'
import { LegList } from './LegList'
import { TicketList } from './TicketList'

const TAG_STYLES: Record<RouteTag, string> = {
  recommended: 'bg-teal-700 text-white',
  fastest: 'bg-sky-100 text-sky-900',
  cheapest: 'bg-emerald-100 text-emerald-900',
  fewest_changes: 'bg-violet-100 text-violet-900',
  most_expensive: 'bg-rose-100 text-rose-900',
}

interface Props {
  route: Route
  index: number
  departAt: string
  selected: boolean
  expanded: boolean
  onSelect: () => void
  onToggle: () => void
  onFocusLeg: (leg: number) => void
}

export function RouteCard({ route, index, departAt, selected, expanded, onSelect, onToggle, onFocusLeg }: Props) {
  const estimated = hasEstimatedFares(route)
  const { metro, road } = fareBreakdown(route)
  const chain = routeChain(route)
  const panelId = `route-${index}-steps`
  const titleId = `route-${index}-title`

  return (
    <article
      aria-labelledby={titleId}
      className={`overflow-hidden rounded-xl border bg-white transition-shadow ${
        selected ? 'border-teal-600 shadow-md ring-1 ring-teal-600' : 'border-slate-200 shadow-sm hover:shadow'
      }`}
    >
      <button
        type="button"
        onClick={onSelect}
        aria-pressed={selected}
        aria-label={`Route ${index + 1}: ${route.summary}. ${formatDuration(route.total_min)}, ${formatRupees(route.fare_rs)}${estimated ? ' estimated' : ''}. Show on map`}
        className="block w-full px-4 pt-3 pb-2 text-left focus-visible:bg-teal-50/60 focus-visible:outline-none"
      >
        {(route.tags.length > 0 || route.dominated || isDirectRide(route)) && (
          <div className="mb-2 flex flex-wrap gap-1">
            {route.tags.map((t) => (
              <span key={t} className={`rounded-full px-2 py-0.5 text-[11px] font-semibold ${TAG_STYLES[t] ?? 'bg-slate-100'}`}>
                {tagLabel(t)}
              </span>
            ))}
            {route.dominated && (
              <span className="rounded-full bg-slate-100 px-2 py-0.5 text-[11px] font-medium text-slate-600">
                Shown for comparison
              </span>
            )}
          </div>
        )}
        <div className="flex items-start justify-between gap-3">
          <div>
            <h3 id={titleId} className="text-lg leading-tight font-bold text-slate-900">
              {formatDuration(route.total_min)}
            </h3>
            <p className="text-xs text-slate-500 tabular-nums">
              {formatClock(departAt)} → {formatClock(departAt, route.total_min)}
            </p>
          </div>
          <div className="text-right">
            <p className="text-lg leading-tight font-bold text-slate-900 tabular-nums">{formatRupees(route.fare_rs)}</p>
            <p className="text-xs text-slate-500">
              {estimated ? (
                <span className="text-amber-800">
                  {metro > 0 ? `metro ${formatRupees(metro)} + ` : ''}est. {formatRupees(road)}
                </span>
              ) : (
                'metro fare'
              )}
            </p>
          </div>
        </div>
        <ol className="mt-2 flex flex-wrap items-center gap-1" aria-label="Route">
          {chain.map((step, i) => (
            <li key={step.key} className="flex items-center gap-1">
              {i > 0 && (
                <span className="text-slate-400" aria-hidden="true">
                  ›
                </span>
              )}
              {step.kind === 'metro' ? (
                <span className="rounded px-1.5 py-0.5 text-[11px] font-semibold" style={{ backgroundColor: step.color, color: textOn(step.color) }}>
                  {step.label}
                </span>
              ) : (
                <span className="rounded border border-slate-200 px-1.5 py-0.5 text-[11px] font-medium text-slate-700">
                  {step.label}
                </span>
              )}
            </li>
          ))}
        </ol>
        <p className="mt-2 text-xs text-slate-600">
          {formatDistance(route.distance_km)} · {formatChanges(route.transfers)}
          {route.walk_min > 0 && ` · ${formatDuration(route.walk_min)} walking`}
        </p>
      </button>
      <button
        type="button"
        onClick={onToggle}
        aria-expanded={expanded}
        aria-controls={panelId}
        className="flex w-full items-center justify-center gap-1 border-t border-slate-100 py-1.5 text-xs font-semibold text-teal-800 hover:bg-slate-50 focus-visible:bg-teal-50 focus-visible:outline-none"
      >
        {expanded ? 'Hide steps' : 'Show steps and prices'}
        <ChevronDown className={`size-3.5 transition-transform ${expanded ? 'rotate-180' : ''}`} aria-hidden="true" />
      </button>
      <div id={panelId} hidden={!expanded} className="border-t border-slate-100 px-4 py-3">
        {expanded && (
          <>
            <LegList route={route} departAt={departAt} onFocusLeg={onFocusLeg} />
            <TicketList tickets={route.tickets} />
          </>
        )}
      </div>
    </article>
  )
}
