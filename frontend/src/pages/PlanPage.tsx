import { CircleAlert, Construction, Pencil } from 'lucide-react'
import { useCallback, useEffect, useRef, useState } from 'react'
import { ApiError, isAbortError, planJourney, searchPlaces } from '../api/client'
import { ClosurePanel } from '../components/ClosurePanel'
import { JourneyForm } from '../components/JourneyForm'
import { MapLegend } from '../components/MapLegend'
import { MapScreen } from '../components/MapScreen'
import { MapView } from '../components/MapView'
import { Results, type ResultState } from '../components/Results'
import type { ClosuresApi } from '../hooks/useClosures'
import type { NetworkState } from '../hooks/useMetroNetwork'
import { findPlace, type ExampleTrip } from '../lib/examples'
import { formatClock, nextQuarterHour, toLocalInputValue } from '../lib/format'
import { ALL_MODES, toJourneyRequest, type PlanIntent, type Query } from '../lib/query'
import type { Snap } from '../lib/sheet'

const NO_LINES = new Map()

interface Props {
  network: NetworkState
  closures: ClosuresApi
  /** A trip started from the home page. */
  intent: PlanIntent | null
}

export function PlanPage({ network, closures, intent }: Props) {
  const [query, setQuery] = useState<Query>(() => ({
    from: null,
    to: null,
    departAt: toLocalInputValue(nextQuarterHour(new Date())),
    modes: [...ALL_MODES],
    preference: 0.5,
  }))
  const [result, setResult] = useState<ResultState>({ status: 'idle' })
  const [selected, setSelected] = useState(0)
  const [expanded, setExpanded] = useState<number | null>(null)
  const [focusLeg, setFocusLeg] = useState<number | null>(null)
  const [snap, setSnap] = useState<Snap>('full')
  // On phones the form folds into a one-line summary once routes are showing.
  const [editing, setEditing] = useState(true)
  const inflight = useRef<AbortController | null>(null)
  const timer = useRef<number | undefined>(undefined)
  const searched = useRef(false)

  const run = useCallback((q: Query) => {
    const req = toJourneyRequest(q)
    if (!req) return
    window.clearTimeout(timer.current)
    inflight.current?.abort()
    const ctl = new AbortController()
    inflight.current = ctl
    searched.current = true
    setSnap((s) => (s === 'peek' ? 'half' : s))
    setResult((prev) => ({
      status: 'loading',
      previous: prev.status === 'success' ? prev.data : prev.status === 'loading' ? prev.previous : null,
    }))
    planJourney(req, ctl.signal)
      .then((data) => {
        setResult({ status: 'success', data })
        setSelected(0)
        setExpanded(null)
        setFocusLeg(null)
        setEditing(false)
        setSnap('half')
      })
      .catch((err: unknown) => {
        if (isAbortError(err)) return
        const e = err instanceof ApiError ? err : new ApiError('unknown', 'Something went wrong. Please try again.')
        setResult({ status: 'error', code: e.code, message: e.message })
        setEditing(true)
      })
  }, [])

  // Callers show the loading state first; this looks the places up and plans the trip.
  const loadExample = useCallback(
    (trip: ExampleTrip, base: Query) =>
      Promise.all([findPlace(trip.from, searchPlaces), findPlace(trip.to, searchPlaces)])
        .then(([from, to]) => {
          if (!from || !to) throw new ApiError('not_found', 'That example trip is not available right now.')
          const next = { ...base, from, to }
          setQuery(next)
          run(next)
        })
        .catch((err: unknown) => {
          const e = err instanceof ApiError ? err : new ApiError('unknown', 'Something went wrong.')
          setResult({ status: 'error', code: e.code, message: e.message })
        }),
    [run],
  )

  // A trip handed over from the home page: fill it in while rendering, then search in an effect.
  const [adopted, setAdopted] = useState(0)
  if (intent && intent.id !== adopted) {
    setAdopted(intent.id)
    if ('example' in intent) {
      setResult({ status: 'loading', previous: null })
    } else {
      setQuery({ ...query, from: intent.from, to: intent.to })
      setEditing(true)
    }
  }
  const searchedIntent = useRef(0)
  useEffect(() => {
    if (!intent || intent.id !== adopted || intent.id === searchedIntent.current) return
    searchedIntent.current = intent.id
    if ('example' in intent) void loadExample(intent.example, query)
    // Sending the request is what this effect is for; run() only marks the results as loading first.
    // eslint-disable-next-line react-hooks/set-state-in-effect
    else run(query)
  }, [intent, adopted, query, run, loadExample])

  // A station or section was closed or reopened: re-plan the current journey around it.
  const seenVersion = useRef(closures.version)
  useEffect(() => {
    if (seenVersion.current === closures.version) return
    const firstLoad = seenVersion.current === -1
    seenVersion.current = closures.version
    if (!firstLoad && searched.current) run(query)
  }, [closures.version, query, run])

  useEffect(
    () => () => {
      inflight.current?.abort()
      window.clearTimeout(timer.current)
      // The aborted search may have been a hand-over: let it run again if the page remounts.
      searchedIntent.current = 0
    },
    [],
  )

  // After the first search, changing the trip re-plans it: toggles at once, sliders after a pause.
  function update(patch: Partial<Query>, opts: { delay?: number } = {}) {
    const next = { ...query, ...patch }
    setQuery(next)
    if (!searched.current || !toJourneyRequest(next)) return
    window.clearTimeout(timer.current)
    if (opts.delay) timer.current = window.setTimeout(() => run(next), opts.delay)
    else run(next)
  }

  const data = result.status === 'success' ? result.data : result.status === 'loading' ? result.previous : null
  const route = data?.routes[selected] ?? null
  const lineById = network.status === 'ready' ? network.network.lineById : NO_LINES
  const folded = !editing && !!query.from && !!query.to

  const panel = (
    <div className="space-y-4 p-4 lg:p-5">
      <section aria-labelledby="plan-heading">
        <h1 id="plan-heading" className={`mb-3 text-lg font-semibold tracking-tight text-ink ${folded ? 'sr-only lg:not-sr-only' : ''}`}>
          Plan a journey
        </h1>
        {folded && (
          <button
            type="button"
            onClick={() => {
              setEditing(true)
              setSnap('full')
            }}
            className="flex w-full items-center gap-3 rounded-control border border-line bg-subtle px-3.5 py-2.5 text-left lg:hidden"
          >
            <span className="flex flex-col items-center gap-0.5 self-stretch py-1" aria-hidden="true">
              <span className="size-2.5 rounded-full border-2 border-brand" />
              <span className="w-px flex-1 border-l-2 border-dotted border-line-strong" />
              <span className="size-2.5 rounded-full bg-accent" />
            </span>
            <span className="min-w-0 flex-1">
              <span className="block truncate text-sm font-semibold text-ink">{query.from?.name}</span>
              <span className="block truncate text-sm font-semibold text-ink">{query.to?.name}</span>
              <span className="block text-xs text-muted">Leave at {formatClock(query.departAt)}</span>
            </span>
            <span className="inline-flex h-9 items-center gap-1 rounded-full bg-surface px-3 text-xs font-semibold text-brand-ink shadow-card">
              <Pencil className="size-3.5" aria-hidden="true" />
              Edit
            </span>
          </button>
        )}
        <div className={folded ? 'hidden lg:block' : undefined}>
          <JourneyForm query={query} onChange={update} onSubmit={() => run(query)} busy={result.status === 'loading'} lineById={lineById} />
        </div>
      </section>
      <ClosurePanel closures={closures} network={network.status === 'ready' ? network.network : null} />
      {data && data.closures > 0 && (
        <p role="status" className="flex items-center gap-2 rounded-xl bg-warn-soft px-3 py-2 text-xs text-warn-ink">
          <Construction className="size-4 shrink-0" aria-hidden="true" />
          Routes avoid {data.closures === 1 ? '1 line closure' : `${data.closures} line closures`}.
        </p>
      )}
      <Results
        state={result}
        selected={selected}
        expanded={expanded}
        onSelect={(i) => {
          setSelected(i)
          setFocusLeg(null)
          setSnap((s) => (s === 'full' ? 'half' : s))
        }}
        onToggle={(i) => setExpanded((e) => (e === i ? null : i))}
        onFocusLeg={(r, leg) => {
          setSelected(r)
          setFocusLeg(leg)
          setSnap('peek')
        }}
        onRetry={() => run(query)}
        onExample={(trip) => {
          setResult({ status: 'loading', previous: null })
          void loadExample(trip, query)
        }}
      />
    </div>
  )

  const map = (
    <>
      <MapView
        network={network.status === 'ready' ? network.network : null}
        route={route}
        focusLeg={focusLeg}
        from={query.from}
        to={query.to}
        closures={closures}
      />
      <MapLegend />
      {network.status === 'error' && (
        <div
          role="status"
          className="absolute top-20 right-3 left-3 z-[1000] flex items-center gap-2 rounded-xl border border-warn-line bg-warn-soft px-3 py-2 text-xs text-warn-ink shadow-card lg:top-3 lg:left-auto"
        >
          <CircleAlert className="size-4 shrink-0" aria-hidden="true" />
          Metro map unavailable: {network.message}
        </div>
      )}
    </>
  )

  return <MapScreen label="Journey planner" panel={panel} map={map} snap={snap} onSnap={setSnap} />
}
