import { CircleAlert, Construction } from 'lucide-react'
import { useCallback, useEffect, useRef, useState } from 'react'
import { ApiError, isAbortError, planJourney, searchPlaces } from '../api/client'
import type { SearchResult } from '../api/types'
import { ClosurePanel } from '../components/ClosurePanel'
import { JourneyForm } from '../components/JourneyForm'
import { MapLegend } from '../components/MapLegend'
import { MapView } from '../components/MapView'
import { Results, type ResultState } from '../components/Results'
import type { ClosuresApi } from '../hooks/useClosures'
import type { NetworkState } from '../hooks/useMetroNetwork'
import { nextQuarterHour, toLocalInputValue } from '../lib/format'
import { ALL_MODES, toJourneyRequest, type Query } from '../lib/query'

const NO_LINES = new Map()

async function findExact(name: string, id: string): Promise<SearchResult | null> {
  const items = await searchPlaces(name)
  return items.find((p) => p.id === id) ?? null
}

export function PlanPage({ network, closures }: { network: NetworkState; closures: ClosuresApi }) {
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
      })
      .catch((err: unknown) => {
        if (isAbortError(err)) return
        const e = err instanceof ApiError ? err : new ApiError('unknown', 'Something went wrong. Please try again.')
        setResult({ status: 'error', code: e.code, message: e.message })
      })
  }, [])

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

  async function loadExample() {
    try {
      const [from, to] = await Promise.all([
        findExact('Bennett University', 'bennett-university'),
        findExact('Hauz Khas Village', 'hauz-khas-village'),
      ])
      if (!from || !to) return
      const next = { ...query, from, to }
      setQuery(next)
      run(next)
    } catch (err) {
      const e = err instanceof ApiError ? err : new ApiError('unknown', 'Something went wrong.')
      setResult({ status: 'error', code: e.code, message: e.message })
    }
  }

  const data = result.status === 'success' ? result.data : result.status === 'loading' ? result.previous : null
  const route = data?.routes[selected] ?? null
  const lineById = network.status === 'ready' ? network.network.lineById : NO_LINES

  return (
    <div className="flex flex-col lg:grid lg:h-full lg:min-h-0 lg:grid-cols-[minmax(340px,430px)_1fr]">
      <div className="contents lg:flex lg:min-h-0 lg:flex-col lg:overflow-y-auto lg:border-r lg:border-slate-200">
        <section aria-labelledby="plan-heading" className="order-1 border-b border-slate-200 bg-white p-4">
          <h1 id="plan-heading" className="mb-3 text-base font-bold text-slate-900">
            Plan a journey
          </h1>
          <JourneyForm
            query={query}
            onChange={update}
            onSubmit={() => run(query)}
            busy={result.status === 'loading'}
            lineById={lineById}
          />
        </section>
        <div className="order-3 space-y-3 p-4 lg:flex-1">
          <ClosurePanel closures={closures} network={network.status === 'ready' ? network.network : null} />
          {data && data.closures > 0 && (
            <p role="status" className="flex items-center gap-2 rounded-md bg-amber-50 px-3 py-2 text-xs text-amber-900">
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
            }}
            onToggle={(i) => setExpanded((e) => (e === i ? null : i))}
            onFocusLeg={(r, leg) => {
              setSelected(r)
              setFocusLeg(leg)
            }}
            onRetry={() => run(query)}
            onExample={() => void loadExample()}
          />
        </div>
      </div>
      <section aria-label="Map" className="relative order-2 h-[55vh] min-h-80 lg:order-none lg:h-full">
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
            className="absolute top-3 right-3 left-14 z-[1000] flex items-center gap-2 rounded-lg border border-amber-300 bg-amber-50 px-3 py-2 text-xs text-amber-900 shadow sm:left-auto"
          >
            <CircleAlert className="size-4 shrink-0" aria-hidden="true" />
            Metro map unavailable: {network.message}
          </div>
        )}
      </section>
    </div>
  )
}
