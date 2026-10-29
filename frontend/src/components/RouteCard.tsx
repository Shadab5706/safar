import { ChevronDown } from 'lucide-react'
import type { Route, RouteTag } from '../api/types'
import { formatChanges, formatClock, formatDistance, formatDuration, formatRupees, tagLabel } from '../lib/format'
import { fareBreakdown, hasEstimatedFares, isDirectRide, routeChain, textOn, timeShares, uiLegColor } from '../lib/route'
import { LegList } from './LegList'
import { TicketList } from './TicketList'

const TAG_STYLES: Record<RouteTag, string> = {
  recommended: 'bg-brand text-on-brand',
  fastest: 'bg-sky-100 text-sky-900 dark:bg-sky-400/15 dark:text-sky-200',
  cheapest: 'bg-emerald-100 text-emerald-900 dark:bg-emerald-400/15 dark:text-emerald-200',
  fewest_changes: 'bg-violet-100 text-violet-900 dark:bg-violet-400/15 dark:text-violet-200',
  most_expensive: 'bg-rose-100 text-rose-900 dark:bg-rose-400/15 dark:text-rose-200',
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
  const shares = timeShares(route)
  const panelId = `route-${index}-steps`
  const titleId = `route-${index}-title`

  return (
    <article
      aria-labelledby={titleId}
      className={`overflow-hidden rounded-card border bg-surface transition-[box-shadow,border-color] duration-200 ${
        selected ? 'border-brand shadow-lift ring-1 ring-brand' : 'border-line shadow-card hover:border-line-strong hover:shadow-lift'
      }`}
    >
      <button
        type="button"
        onClick={onSelect}
        aria-pressed={selected}
        aria-label={`Route ${index + 1}: ${route.summary}. ${formatDuration(route.total_min)}, ${formatRupees(route.fare_rs)}${estimated ? ' estimated' : ''}. Show on map`}
        className="block w-full px-4 pt-3.5 pb-3 text-left focus-visible:bg-brand-soft/50 focus-visible:outline-none"
      >
        {(route.tags.length > 0 || route.dominated || isDirectRide(route)) && (
          <div className="mb-2.5 flex flex-wrap gap-1">
            {route.tags.map((t) => (
              <span key={t} className={`rounded-full px-2 py-0.5 text-[11px] font-semibold ${TAG_STYLES[t] ?? 'bg-subtle'}`}>
                {tagLabel(t)}
              </span>
            ))}
            {route.dominated && (
              <span className="rounded-full bg-subtle px-2 py-0.5 text-[11px] font-medium text-muted">Shown for comparison</span>
            )}
          </div>
        )}
        <div className="flex items-start justify-between gap-3">
          <div>
            <h3 id={titleId} className="text-xl leading-tight font-semibold tracking-tight text-ink tabular-nums">
              {formatDuration(route.total_min)}
            </h3>
            <p className="mt-0.5 text-xs text-muted tabular-nums">
              {formatClock(departAt)} – {formatClock(departAt, route.total_min)}
            </p>
          </div>
          <div className="text-right">
            <p className="text-xl leading-tight font-semibold tracking-tight text-ink tabular-nums">{formatRupees(route.fare_rs)}</p>
            <p className="mt-0.5 text-xs text-muted">
              {estimated ? (
                <span className="text-warn-ink">
                  {metro > 0 ? `metro ${formatRupees(metro)} + ` : ''}est. {formatRupees(road)}
                </span>
              ) : (
                'metro fare'
              )}
            </p>
          </div>
        </div>

        {/* Each leg's share of the journey time, in its own colour. */}
        <div className="mt-3 flex h-1.5 gap-0.5 overflow-hidden rounded-full" aria-hidden="true">
          {shares.map((s) => (
            <span key={s.index} className="h-full rounded-full" style={{ flexGrow: s.share, backgroundColor: uiLegColor(route.legs[s.index]!) }} />
          ))}
        </div>

        <ol className="mt-2.5 flex flex-wrap items-center gap-1" aria-label="Route">
          {chain.map((step, i) => (
            <li key={step.key} className="flex items-center gap-1">
              {i > 0 && (
                <span className="text-faint" aria-hidden="true">
                  ›
                </span>
              )}
              {step.kind === 'metro' ? (
                <span className="rounded-md px-1.5 py-0.5 text-[11px] font-semibold" style={{ backgroundColor: step.color, color: textOn(step.color) }}>
                  {step.label}
                </span>
              ) : (
                <span className="rounded-md bg-subtle px-1.5 py-0.5 text-[11px] font-medium text-ink-2">{step.label}</span>
              )}
            </li>
          ))}
        </ol>
        <p className="mt-2.5 text-xs text-muted">
          {formatDistance(route.distance_km)} · {formatChanges(route.transfers)}
          {route.walk_min > 0 && ` · ${formatDuration(route.walk_min)} walking`}
        </p>
      </button>
      <button
        type="button"
        onClick={onToggle}
        aria-expanded={expanded}
        aria-controls={panelId}
        className="flex h-11 w-full items-center justify-center gap-1 border-t border-line text-sm font-semibold text-brand-ink transition-colors hover:bg-subtle focus-visible:bg-brand-soft focus-visible:outline-none"
      >
        {expanded ? 'Hide steps' : 'Steps and prices'}
        <ChevronDown className={`size-4 transition-transform duration-200 ${expanded ? 'rotate-180' : ''}`} aria-hidden="true" />
      </button>
      <div id={panelId} hidden={!expanded} className="border-t border-line bg-subtle/40 px-4 py-4">
        {expanded && (
          <div className="animate-fade">
            <LegList route={route} departAt={departAt} onFocusLeg={onFocusLeg} />
            <TicketList tickets={route.tickets} />
          </div>
        )}
      </div>
    </article>
  )
}
