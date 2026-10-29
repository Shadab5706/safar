import { ArrowRight, CircleAlert, Info } from 'lucide-react'
import type { JourneyResponse } from '../api/types'
import { EXAMPLE_TRIPS, type ExampleTrip } from '../lib/examples'
import { RouteCard } from './RouteCard'

export type ResultState =
  | { status: 'idle' }
  | { status: 'loading'; previous: JourneyResponse | null }
  | { status: 'success'; data: JourneyResponse }
  | { status: 'error'; code: string; message: string }

interface Props {
  state: ResultState
  selected: number
  expanded: number | null
  onSelect: (i: number) => void
  onToggle: (i: number) => void
  onFocusLeg: (route: number, leg: number) => void
  onRetry: () => void
  onExample: (trip: ExampleTrip) => void
}

function Skeleton() {
  return (
    <div className="space-y-3" aria-hidden="true">
      {[0, 1, 2].map((i) => (
        <div key={i} className="animate-pulse rounded-card border border-line bg-surface p-4 shadow-card">
          <div className="mb-3 h-4 w-24 rounded-full bg-subtle-2" />
          <div className="flex justify-between">
            <div className="h-6 w-28 rounded bg-subtle-2" />
            <div className="h-6 w-16 rounded bg-subtle-2" />
          </div>
          <div className="mt-3 h-4 w-3/4 rounded bg-subtle" />
        </div>
      ))}
    </div>
  )
}

export function Results({ state, selected, expanded, onSelect, onToggle, onFocusLeg, onRetry, onExample }: Props) {
  if (state.status === 'idle') {
    return (
      <div className="animate-fade">
        <h2 className="text-xs font-semibold tracking-wide text-muted uppercase">Try a trip</h2>
        <ul className="mt-2 space-y-2">
          {EXAMPLE_TRIPS.map((trip) => (
            <li key={trip.from.id + trip.to.id}>
              <button
                type="button"
                onClick={() => onExample(trip)}
                className="group flex min-h-14 w-full items-center gap-3 rounded-control border border-line bg-surface px-3.5 py-2.5 text-left shadow-card transition hover:border-brand/50 hover:shadow-lift focus-visible:outline-2 focus-visible:outline-brand"
              >
                <span className="min-w-0 flex-1">
                  <span className="block truncate text-sm font-semibold text-ink">
                    {trip.from.name} → {trip.to.name}
                  </span>
                  <span className="block text-xs text-muted">{trip.note}</span>
                </span>
                <ArrowRight className="size-4 shrink-0 text-faint transition-transform group-hover:translate-x-0.5 group-hover:text-brand-ink" aria-hidden="true" />
              </button>
            </li>
          ))}
        </ul>
      </div>
    )
  }

  if (state.status === 'loading' && !state.previous) {
    return (
      <div aria-busy="true">
        <p className="sr-only" role="status">
          Finding routes…
        </p>
        <Skeleton />
      </div>
    )
  }

  if (state.status === 'error') {
    const noRoute = state.code === 'no_route'
    return (
      <div
        role="alert"
        className={`rounded-card border p-4 ${noRoute ? 'border-line bg-surface' : 'border-danger-line bg-danger-soft'}`}
      >
        <div className="flex gap-2">
          <CircleAlert className={`mt-0.5 size-5 shrink-0 ${noRoute ? 'text-muted' : 'text-danger-ink'}`} aria-hidden="true" />
          <div>
            <p className={`text-sm font-semibold ${noRoute ? 'text-ink' : 'text-danger-ink'}`}>
              {noRoute ? 'No route with these modes' : 'Could not plan this journey'}
            </p>
            <p className={`mt-0.5 text-sm ${noRoute ? 'text-muted' : 'text-danger-ink'}`}>
              {noRoute ? 'Try turning on metro or a cab, auto or bike taxi.' : state.message}
            </p>
            {!noRoute && (
              <button
                type="button"
                onClick={onRetry}
                className="mt-3 inline-flex h-10 items-center rounded-full bg-danger px-4 text-sm font-semibold text-white hover:bg-danger-strong focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-danger"
              >
                Try again
              </button>
            )}
          </div>
        </div>
      </div>
    )
  }

  const data = state.status === 'success' ? state.data : state.previous
  if (!data) return null
  const busy = state.status === 'loading'

  return (
    <section aria-label="Routes" aria-busy={busy} className={busy ? 'opacity-60 transition-opacity' : ''}>
      <p className="sr-only" role="status">
        {busy ? 'Updating routes…' : `${data.routes.length} routes found.`}
      </p>
      <div className="mb-3 flex items-baseline justify-between gap-2">
        <h2 className="text-base font-semibold tracking-tight text-ink">
          {data.routes.length} {data.routes.length === 1 ? 'route' : 'routes'}
        </h2>
        <p className="text-xs text-muted">
          best of {data.stats.candidates} candidates{data.depart.sunday_fares ? ' · Sunday metro fares' : ''}
        </p>
      </div>
      <div className="mb-3 flex gap-2 rounded-xl border border-warn-line bg-warn-soft px-3 py-2.5 text-xs text-warn-ink">
        <Info className="mt-px size-3.5 shrink-0" aria-hidden="true" />
        <p>{data.fare_note}</p>
      </div>
      <ul className="space-y-3">
        {data.routes.map((route, i) => (
          <li key={`${route.rank}:${route.summary}`}>
            <RouteCard
              route={route}
              index={i}
              departAt={data.depart.at}
              selected={i === selected}
              expanded={i === expanded}
              onSelect={() => onSelect(i)}
              onToggle={() => onToggle(i)}
              onFocusLeg={(leg) => onFocusLeg(i, leg)}
            />
          </li>
        ))}
      </ul>
    </section>
  )
}
