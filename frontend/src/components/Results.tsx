import { CircleAlert, Info, Route as RouteIcon } from 'lucide-react'
import type { JourneyResponse } from '../api/types'
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
  onExample: () => void
}

function Skeleton() {
  return (
    <div className="space-y-3" aria-hidden="true">
      {[0, 1, 2].map((i) => (
        <div key={i} className="animate-pulse rounded-xl border border-slate-200 bg-white p-4">
          <div className="mb-3 h-4 w-24 rounded-full bg-slate-200" />
          <div className="flex justify-between">
            <div className="h-6 w-28 rounded bg-slate-200" />
            <div className="h-6 w-16 rounded bg-slate-200" />
          </div>
          <div className="mt-3 h-4 w-3/4 rounded bg-slate-100" />
        </div>
      ))}
    </div>
  )
}

export function Results({ state, selected, expanded, onSelect, onToggle, onFocusLeg, onRetry, onExample }: Props) {
  if (state.status === 'idle') {
    return (
      <div className="rounded-xl border border-dashed border-slate-300 bg-white/60 p-5 text-center">
        <RouteIcon className="mx-auto mb-2 size-7 text-teal-700" aria-hidden="true" />
        <p className="text-sm font-semibold text-slate-900">Where are you going?</p>
        <p className="mt-1 text-sm text-slate-600">
          Pick a start and a destination. Safar mixes metro, cab, auto, bike taxi and walking, and shows the time and
          price of every option.
        </p>
        <button
          type="button"
          onClick={onExample}
          className="mt-3 rounded-lg border border-teal-700 px-3 py-1.5 text-sm font-medium text-teal-800 hover:bg-teal-50 focus-visible:outline-2 focus-visible:outline-teal-600"
        >
          Try Bennett University → Hauz Khas Village
        </button>
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
        className={`rounded-xl border p-4 ${noRoute ? 'border-slate-200 bg-white' : 'border-red-200 bg-red-50'}`}
      >
        <div className="flex gap-2">
          <CircleAlert className={`mt-0.5 size-5 shrink-0 ${noRoute ? 'text-slate-500' : 'text-red-700'}`} aria-hidden="true" />
          <div>
            <p className={`text-sm font-semibold ${noRoute ? 'text-slate-900' : 'text-red-900'}`}>
              {noRoute ? 'No route with these modes' : 'Could not plan this journey'}
            </p>
            <p className={`mt-0.5 text-sm ${noRoute ? 'text-slate-600' : 'text-red-800'}`}>
              {noRoute ? 'Try turning on metro or a cab, auto or bike taxi.' : state.message}
            </p>
            {!noRoute && (
              <button
                type="button"
                onClick={onRetry}
                className="mt-2 rounded-md bg-red-700 px-3 py-1 text-sm font-medium text-white hover:bg-red-800 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-red-700"
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
      <div className="mb-2 flex items-baseline justify-between gap-2">
        <h2 className="text-sm font-semibold text-slate-900">
          {data.routes.length} {data.routes.length === 1 ? 'route' : 'routes'}
        </h2>
        <p className="text-xs text-slate-500">
          best of {data.stats.candidates} candidates{data.depart.sunday_fares ? ' · Sunday metro fares' : ''}
        </p>
      </div>
      <div className="mb-3 flex gap-2 rounded-lg border border-amber-200 bg-amber-50 px-3 py-2 text-xs text-amber-900">
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
