import { ArrowDown, ArrowUp, CircleAlert, LoaderCircle, MapPinned, Route as RouteIcon, X } from 'lucide-react'
import { useCallback, useEffect, useMemo, useRef, useState, type ReactNode } from 'react'
import { ApiError, endpointRef, isAbortError, optimizeTrip, searchPlaces } from '../api/client'
import type { MetroStation, SearchResult, TripMethod, TripRequest, TripResponse } from '../api/types'
import { BaseMap, type StationAction } from '../components/BaseMap'
import { ClosurePanel } from '../components/ClosurePanel'
import { ConvergenceChart } from '../components/ConvergenceChart'
import { PlaceInput } from '../components/PlaceInput'
import { TripLayer, type TripStop } from '../components/TripLayer'
import type { ClosuresApi } from '../hooks/useClosures'
import type { NetworkState } from '../hooks/useMetroNetwork'
import { formatDuration, formatRupees, nextQuarterHour, toLocalInputValue } from '../lib/format'
import { preferenceLabel } from '../lib/query'
import { boundsOf } from '../lib/route'
import { MAX_STOPS, METHODS, MIN_STOPS, methodInfo, moveItem, savings } from '../lib/trip'

type TripState =
  | { status: 'idle' }
  | { status: 'loading'; previous: TripResponse | null; previousKey: string }
  | { status: 'success'; data: TripResponse; key: string }
  | { status: 'error'; code: string; message: string }

const EXAMPLE = [
  ['Connaught Place', 'connaught-place'],
  ['India Gate', 'india-gate'],
  ['Red Fort', 'red-fort'],
  ['Qutub Minar', 'qutub-minar'],
  ['Lotus Temple', 'lotus-temple'],
  ["Humayun's Tomb", 'humayuns-tomb'],
  ['Hauz Khas Village', 'hauz-khas-village'],
  ['Akshardham Temple', 'akshardham-temple'],
] as const

const NO_LINES = new Map()
const keyOf = (s: SearchResult) => endpointRef(s)

function IconButton({ label, onClick, disabled, children }: { label: string; onClick: () => void; disabled?: boolean; children: ReactNode }) {
  return (
    <button
      type="button"
      aria-label={label}
      title={label}
      onClick={onClick}
      disabled={disabled}
      className="inline-flex size-7 items-center justify-center rounded text-slate-600 hover:bg-slate-100 focus-visible:outline-2 focus-visible:outline-teal-700 disabled:opacity-30"
    >
      {children}
    </button>
  )
}

export function TripPage({ network, closures }: { network: NetworkState; closures: ClosuresApi }) {
  const net = network.status === 'ready' ? network.network : null
  const [stops, setStops] = useState<SearchResult[]>([])
  const [roundTrip, setRoundTrip] = useState(false)
  const [method, setMethod] = useState<TripMethod | 'both'>('both')
  const [departAt, setDepartAt] = useState(() => toLocalInputValue(nextQuarterHour(new Date())))
  const [preference, setPreference] = useState(0.5)
  const [state, setState] = useState<TripState>({ status: 'idle' })
  const [adderKey, setAdderKey] = useState(0)
  const [notice, setNotice] = useState<string | null>(null)
  const inflight = useRef<AbortController | null>(null)

  const request: TripRequest = useMemo(
    () => ({
      stops: stops.map(keyOf),
      method,
      round_trip: roundTrip,
      depart_at: departAt,
      preference: Math.round(preference * 100) / 100,
    }),
    [stops, method, roundTrip, departAt, preference],
  )
  const requestKey = JSON.stringify(request)

  const run = useCallback((req: TripRequest) => {
    const key = JSON.stringify(req)
    inflight.current?.abort()
    const ctl = new AbortController()
    inflight.current = ctl
    setState((prev) =>
      prev.status === 'success'
        ? { status: 'loading', previous: prev.data, previousKey: prev.key }
        : prev.status === 'loading'
          ? prev
          : { status: 'loading', previous: null, previousKey: '' },
    )
    optimizeTrip(req, ctl.signal)
      .then((data) => setState({ status: 'success', data, key }))
      .catch((err: unknown) => {
        if (isAbortError(err)) return
        const e = err instanceof ApiError ? err : new ApiError('unknown', 'Something went wrong. Please try again.')
        setState({ status: 'error', code: e.code, message: e.message })
      })
  }, [])

  useEffect(() => () => inflight.current?.abort(), [])

  const data = state.status === 'success' ? state.data : state.status === 'loading' ? state.previous : null
  const dataKey = state.status === 'success' ? state.key : state.status === 'loading' ? state.previousKey : ''
  const stale = data !== null && dataKey !== requestKey
  const busy = state.status === 'loading'

  // Closures changed: re-optimise the current trip around them.
  const seenVersion = useRef(closures.version)
  useEffect(() => {
    if (seenVersion.current === closures.version) return
    const firstLoad = seenVersion.current === -1
    seenVersion.current = closures.version
    if (!firstLoad && data && !stale) run(request)
  }, [closures.version, data, stale, request, run])

  const add = useCallback(
    (item: SearchResult | null) => {
      if (!item) return
      setAdderKey((k) => k + 1)
      if (stops.some((s) => keyOf(s) === keyOf(item))) setNotice(`${item.name} is already on the trip.`)
      else if (stops.length >= MAX_STOPS) setNotice(`A trip can have at most ${MAX_STOPS} stops.`)
      else {
        setNotice(null)
        setStops([...stops, item])
      }
    },
    [stops],
  )

  const stationActions = useCallback(
    (s: MetroStation): StationAction[] => [{ label: 'Add to trip', onClick: () => add({ kind: 'station', ...s }) }],
    [add],
  )

  async function loadExample() {
    setNotice(null)
    try {
      const found = await Promise.all(
        EXAMPLE.map(async ([name, id]) => (await searchPlaces(name)).find((p) => p.kind === 'place' && p.id === id) ?? null),
      )
      const list = found.filter((p): p is SearchResult => p !== null)
      if (list.length < MIN_STOPS) throw new ApiError('example', 'Could not load the example stops.')
      setStops(list)
      run({ ...request, stops: list.map(keyOf) })
    } catch (err) {
      const e = err instanceof ApiError ? err : new ApiError('unknown', 'Something went wrong.')
      setState({ status: 'error', code: e.code, message: e.message })
    }
  }

  // Map: the optimised order when the result matches the stops, otherwise the stops as entered.
  const shown = data && !stale
  const mapStops: TripStop[] = shown
    ? data.best.order.map((si, pos) => {
        const s = data.stops[si]!
        return { key: `${s.type}:${s.id}`, name: s.name, lat: s.lat, lng: s.lng, label: String(pos + 1), start: pos === 0 }
      })
    : stops.map((s, i) => ({ key: keyOf(s), name: s.name, lat: s.lat, lng: s.lng, label: i === 0 ? 'S' : String(i + 1), start: i === 0 }))
  const mapLegs = shown ? data.best.legs.map((l) => l.route.legs) : []
  const bounds = boundsOf(mapStops)
  const fitKey = `${mapStops.map((s) => s.key).join('|')}:${shown ? data.best.order.join(',') : 'input'}`

  const saved = data ? savings(data.input, data.best) : null
  const ready = stops.length >= MIN_STOPS

  return (
    <div className="flex flex-col lg:grid lg:h-full lg:min-h-0 lg:grid-cols-[minmax(360px,480px)_1fr]">
      <div className="contents lg:flex lg:min-h-0 lg:flex-col lg:overflow-y-auto lg:border-r lg:border-slate-200">
        <section aria-labelledby="trip-heading" className="order-1 border-b border-slate-200 bg-white p-4">
          <h1 id="trip-heading" className="mb-1 text-base font-bold text-slate-900">
            Trip planner
          </h1>
          <p className="mb-3 text-xs text-slate-600">
            Add {MIN_STOPS} to {MAX_STOPS} stops. The first one is where you start; Safar finds the order that visits the
            rest for the least time and money.
          </p>

          <form
            className="space-y-3"
            aria-label="Optimise a trip"
            onSubmit={(e) => {
              e.preventDefault()
              if (ready) run(request)
            }}
          >
            <div>
              <PlaceInput
                key={adderKey}
                label={stops.length === 0 ? 'Start from' : 'Add a stop'}
                placeholder="Search places or stations, or click a station on the map"
                value={null}
                onChange={add}
                lineById={net?.lineById ?? NO_LINES}
              />
              {notice && (
                <p role="status" className="mt-1 text-xs text-amber-800">
                  {notice}
                </p>
              )}
            </div>

            {stops.length > 0 && (
              <ol className="divide-y divide-slate-100 rounded-lg border border-slate-200" aria-label="Stops as entered">
                {stops.map((s, i) => (
                  <li key={keyOf(s)} className="flex items-center gap-2 px-2 py-1.5">
                    <span
                      className={`flex size-6 shrink-0 items-center justify-center rounded-full text-[11px] font-bold text-white ${i === 0 ? 'bg-teal-700' : 'bg-slate-700'}`}
                      aria-hidden="true"
                    >
                      {i === 0 ? 'S' : i + 1}
                    </span>
                    <span className="min-w-0 flex-1">
                      <span className="block truncate text-sm text-slate-900">{s.name}</span>
                      <span className="block text-[11px] text-slate-500">{i === 0 ? 'Start' : s.kind === 'station' ? 'Metro station' : 'Place'}</span>
                    </span>
                    <IconButton label={`Move ${s.name} up`} onClick={() => setStops((c) => moveItem(c, i, i - 1))} disabled={i === 0}>
                      <ArrowUp className="size-3.5" aria-hidden="true" />
                    </IconButton>
                    <IconButton label={`Move ${s.name} down`} onClick={() => setStops((c) => moveItem(c, i, i + 1))} disabled={i === stops.length - 1}>
                      <ArrowDown className="size-3.5" aria-hidden="true" />
                    </IconButton>
                    <IconButton label={`Remove ${s.name}`} onClick={() => setStops((c) => c.filter((_, j) => j !== i))}>
                      <X className="size-3.5" aria-hidden="true" />
                    </IconButton>
                  </li>
                ))}
              </ol>
            )}

            <div className="grid grid-cols-1 gap-3 sm:grid-cols-2">
              <label className="block text-xs font-semibold tracking-wide text-slate-600 uppercase">
                Leave at
                <input
                  type="datetime-local"
                  value={departAt}
                  onChange={(e) => setDepartAt(e.target.value)}
                  className="mt-1 w-full rounded-lg border border-slate-300 bg-white px-3 py-2 text-sm font-normal tracking-normal text-slate-900"
                />
              </label>
              <label className="block text-xs font-semibold tracking-wide text-slate-600 uppercase">
                Search method
                <select
                  value={method}
                  onChange={(e) => setMethod(e.target.value as TripMethod | 'both')}
                  className="mt-1 block w-full rounded-lg border border-slate-300 bg-white px-3 py-2 text-sm font-normal tracking-normal text-slate-900 normal-case"
                >
                  <option value="both">Compare both</option>
                  {METHODS.map((m) => (
                    <option key={m.id} value={m.id}>
                      {m.label}
                    </option>
                  ))}
                </select>
              </label>
            </div>

            <div>
              <div className="mb-1 flex items-baseline justify-between">
                <label htmlFor="trip-preference" className="text-xs font-semibold tracking-wide text-slate-600 uppercase">
                  Priority
                </label>
                <span className="text-xs font-medium text-teal-800" aria-hidden="true">
                  {preferenceLabel(preference)}
                </span>
              </div>
              <input
                id="trip-preference"
                type="range"
                min={0}
                max={1}
                step={0.05}
                value={preference}
                onChange={(e) => setPreference(Number(e.target.value))}
                aria-valuetext={preferenceLabel(preference)}
                className="w-full accent-teal-700"
              />
              <div className="flex justify-between text-xs text-slate-500" aria-hidden="true">
                <span>Cheapest</span>
                <span>Fastest</span>
              </div>
            </div>

            <label className="flex items-center gap-2 text-sm text-slate-800">
              <input type="checkbox" checked={roundTrip} onChange={(e) => setRoundTrip(e.target.checked)} className="size-4 accent-teal-700" />
              Come back to the start at the end
            </label>

            <button
              type="submit"
              disabled={!ready || busy}
              className="inline-flex w-full items-center justify-center gap-2 rounded-lg bg-teal-700 px-4 py-2.5 text-[15px] font-semibold text-white shadow-sm hover:bg-teal-800 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-teal-700 disabled:opacity-50"
            >
              {busy ? <LoaderCircle className="size-4 animate-spin" aria-hidden="true" /> : <RouteIcon className="size-4" aria-hidden="true" />}
              {busy ? 'Planning every leg…' : ready ? 'Find the best order' : `Add at least ${MIN_STOPS} stops`}
            </button>
          </form>
        </section>

        <div className="order-3 space-y-3 p-4 lg:flex-1">
          <ClosurePanel closures={closures} network={net} />

          {state.status === 'error' && (
            <div role="alert" className="flex items-start gap-2 rounded-lg border border-red-200 bg-red-50 p-3 text-sm text-red-900">
              <CircleAlert className="mt-0.5 size-4 shrink-0" aria-hidden="true" />
              <div className="flex-1">
                <p>{state.message}</p>
                {ready && (
                  <button type="button" onClick={() => run(request)} className="mt-1 text-xs font-semibold underline">
                    Try again
                  </button>
                )}
              </div>
            </div>
          )}

          {data && saved ? (
            <div className={busy ? 'space-y-4 opacity-60 transition-opacity' : 'space-y-4'} aria-busy={busy}>
              {stale && (
                <p role="status" className="rounded-md bg-amber-50 px-3 py-2 text-xs text-amber-900">
                  The stops or options changed since this plan. Press “Find the best order” to update it.
                </p>
              )}
              <section aria-labelledby="trip-best" className="rounded-lg border border-slate-200 bg-white p-3">
                <h2 id="trip-best" className="text-sm font-bold text-slate-900">
                  Best order
                </h2>
                <p className="mt-0.5 text-xs text-slate-600">
                  {formatDuration(data.best.total_min)} · {formatRupees(data.best.fare_rs)} in fares
                  {saved.pct > 0
                    ? ` · ${saved.pct}% lower cost than the order you entered (${saved.min > 0 ? `${formatDuration(saved.min)} faster` : 'same time'}${saved.rs > 0 ? `, ${formatRupees(saved.rs)} cheaper` : ''})`
                    : ' · the order you entered was already the best'}
                </p>
                <ol className="mt-3 space-y-0">
                  {data.best.order.map((si, pos) => {
                    const stop = data.stops[si]!
                    const leg = data.best.legs[pos]
                    return (
                      <li key={`${si}-${pos}`} className="relative pb-2 pl-8">
                        <span
                          className={`absolute top-0 left-0 flex size-6 items-center justify-center rounded-full text-[11px] font-bold text-white ${pos === 0 ? 'bg-teal-700' : 'bg-slate-800'}`}
                          aria-hidden="true"
                        >
                          {pos + 1}
                        </span>
                        <p className="text-sm font-medium text-slate-900">
                          <span className="sr-only">Stop {pos + 1}: </span>
                          {stop.name}
                        </p>
                        {leg && (
                          <p className="mt-0.5 border-l-2 border-dashed border-slate-300 py-1 pl-3 text-xs text-slate-600">
                            {leg.route.summary} · {formatDuration(leg.route.total_min)} · {formatRupees(leg.route.fare_rs)}
                            {leg.to === data.best.order[0] && data.round_trip ? ` · back to ${data.stops[leg.to]?.name}` : ''}
                          </p>
                        )}
                      </li>
                    )
                  })}
                </ol>
              </section>

              <section aria-labelledby="trip-methods">
                <h2 id="trip-methods" className="mb-2 text-sm font-bold text-slate-900">
                  How the searches did
                </h2>
                <div className="overflow-x-auto rounded-lg border border-slate-200 bg-white">
                  <table className="w-full min-w-[420px] text-left text-xs">
                    <caption className="sr-only">Local search results per method</caption>
                    <thead className="bg-slate-50 text-[11px] tracking-wide text-slate-600 uppercase">
                      <tr>
                        <th scope="col" className="px-2 py-1.5 font-semibold">Method</th>
                        <th scope="col" className="px-2 py-1.5 text-right font-semibold">Cost</th>
                        <th scope="col" className="px-2 py-1.5 text-right font-semibold">Gap</th>
                        <th scope="col" className="px-2 py-1.5 text-right font-semibold">Tours tried</th>
                        <th scope="col" className="px-2 py-1.5 text-right font-semibold">Time</th>
                      </tr>
                    </thead>
                    <tbody className="divide-y divide-slate-100 tabular-nums">
                      {data.results.map((r) => (
                        <tr key={r.method}>
                          <th scope="row" className="px-2 py-1.5 font-medium text-slate-900">
                            <span className="inline-flex items-center gap-1.5">
                              <span className="size-2.5 rounded-full" style={{ background: methodInfo(r.method).color }} aria-hidden="true" />
                              {methodInfo(r.method).label}
                            </span>
                            <span className="block pl-4 text-[11px] font-normal text-slate-500">
                              {r.method === 'hill_climb' ? `${r.restarts} climbs, ${r.moves} moves` : `${r.moves} moves, ${r.accepted_worse} uphill`}
                            </span>
                          </th>
                          <td className="px-2 py-1.5 text-right">{r.score.toFixed(1)}</td>
                          <td className={`px-2 py-1.5 text-right ${r.gap_pct === 0 ? 'font-semibold text-teal-800' : ''}`}>
                            {r.gap_pct === undefined ? '–' : r.gap_pct === 0 ? 'optimal' : `+${r.gap_pct.toFixed(1)}%`}
                          </td>
                          <td className="px-2 py-1.5 text-right">{r.evaluations.toLocaleString('en-IN')}</td>
                          <td className="px-2 py-1.5 text-right">{r.runtime_ms.toFixed(2)} ms</td>
                        </tr>
                      ))}
                      {data.optimum && (
                        <tr className="text-slate-600">
                          <th scope="row" className="px-2 py-1.5 font-medium">
                            Every order (exact)
                          </th>
                          <td className="px-2 py-1.5 text-right">{data.optimum.score.toFixed(1)}</td>
                          <td className="px-2 py-1.5 text-right">–</td>
                          <td className="px-2 py-1.5 text-right">{data.optimum.evaluations.toLocaleString('en-IN')}</td>
                          <td className="px-2 py-1.5 text-right">{data.optimum.runtime_ms.toFixed(2)} ms</td>
                        </tr>
                      )}
                    </tbody>
                  </table>
                </div>
              </section>

              <section aria-label="Convergence chart" className="rounded-lg border border-slate-200 bg-white p-3">
                <ConvergenceChart results={data.results} optimum={data.optimum?.score ?? null} />
              </section>

              <p className="text-[11px] text-slate-500">
                Cost of a leg = minutes + rupees ÷ ₹{data.value_of_time_rs_per_min} per minute (your priority
                setting). Each leg is the recommended journey from the planner; {data.stops.length * (data.stops.length - 1)}{' '}
                journeys planned in {Math.round(data.matrix_ms)} ms
                {data.closures > 0 ? `, avoiding ${data.closures} line closure${data.closures === 1 ? '' : 's'}` : ''}. Cab, auto
                and bike fares are estimates.
              </p>
            </div>
          ) : (
            state.status !== 'error' && (
              <div className="rounded-lg border border-dashed border-slate-300 bg-white p-5 text-center">
                <MapPinned className="mx-auto size-8 text-teal-700" aria-hidden="true" />
                <p className="mt-2 text-sm font-semibold text-slate-900">Visit several places in one day</p>
                <p className="mt-1 text-xs text-slate-600">
                  Hill climbing and simulated annealing search for the visiting order with the lowest total cost, and the
                  chart shows how each one converges.
                </p>
                <button
                  type="button"
                  onClick={() => void loadExample()}
                  disabled={busy}
                  className="mt-3 rounded-md px-3 py-1.5 text-sm font-semibold text-teal-800 hover:bg-teal-50 disabled:opacity-50"
                >
                  Try a Delhi sightseeing day (8 stops)
                </button>
              </div>
            )
          )}
        </div>
      </div>

      <section aria-label="Map" className="relative order-2 h-[55vh] min-h-80 lg:order-none lg:h-full">
        <BaseMap network={net} closures={closures} stationActions={stationActions} bounds={bounds} fitKey={fitKey}>
          <TripLayer key={fitKey} stops={mapStops} legs={mapLegs} />
        </BaseMap>
        {network.status === 'error' && (
          <div role="status" className="absolute top-3 right-3 left-14 z-[1000] flex items-center gap-2 rounded-lg border border-amber-300 bg-amber-50 px-3 py-2 text-xs text-amber-900 shadow sm:left-auto">
            <CircleAlert className="size-4 shrink-0" aria-hidden="true" />
            Metro map unavailable: {network.message}
          </div>
        )}
      </section>
    </div>
  )
}
