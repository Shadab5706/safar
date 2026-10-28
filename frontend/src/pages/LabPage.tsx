import { CircleAlert, FlaskConical, LoaderCircle, Play } from 'lucide-react'
import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { ApiError, compareAlgorithms, isAbortError } from '../api/client'
import type { Algorithm, LabResponse, MetroStation, SearchResult } from '../api/types'
import { BaseMap, type StationAction } from '../components/BaseMap'
import { ClosurePanel } from '../components/ClosurePanel'
import { ExpansionLayer } from '../components/ExpansionLayer'
import { LabResults } from '../components/LabResults'
import { PlaceInput } from '../components/PlaceInput'
import type { ClosuresApi } from '../hooks/useClosures'
import type { NetworkState } from '../hooks/useMetroNetwork'
import { usePlayer } from '../hooks/usePlayer'
import { ALGORITHMS, algorithmInfo, expansionFrames, framesUpTo } from '../lib/lab'
import { boundsOf } from '../lib/route'

type LabState =
  | { status: 'idle' }
  | { status: 'loading'; previous: LabResponse | null }
  | { status: 'success'; data: LabResponse }
  | { status: 'error'; message: string }

const HEURISTICS = [
  { value: 0, label: 'Admissible (fastest train speed)' },
  { value: 60, label: '60 km/h (overestimates)' },
  { value: 30, label: '30 km/h (overestimates a lot)' },
] as const

const EXAMPLE = { from: 'samaypur-badli', to: 'botanical-garden' }
const NO_LINES = new Map()
const NO_EXPANSION: string[] = []

function asResult(s: MetroStation): SearchResult {
  return { kind: 'station', ...s }
}

export function LabPage({ network, closures }: { network: NetworkState; closures: ClosuresApi }) {
  const net = network.status === 'ready' ? network.network : null
  const stationById = useMemo(() => new Map((net?.stations ?? []).map((s) => [s.id, s])), [net])
  const [from, setFrom] = useState<SearchResult | null>(null)
  const [to, setTo] = useState<SearchResult | null>(null)
  const [algos, setAlgos] = useState<Algorithm[]>(() => ALGORITHMS.map((a) => a.id))
  const [heuristic, setHeuristic] = useState<number>(0)
  const [state, setState] = useState<LabState>({ status: 'idle' })
  const [active, setActive] = useState<Algorithm>('astar')
  const [formError, setFormError] = useState<string | null>(null)
  const inflight = useRef<AbortController | null>(null)

  const data = state.status === 'success' ? state.data : state.status === 'loading' ? state.previous : null
  const result = data?.results.find((r) => r.algorithm === active) ?? data?.results[0]
  const expansion = result?.expansion ?? NO_EXPANSION
  const frames = useMemo(() => expansionFrames(expansion), [expansion])
  const player = usePlayer(expansion.length)
  const visible = framesUpTo(frames, player.step)
  const finished = expansion.length > 0 && player.step >= expansion.length - 1
  const { restart } = player

  const run = useCallback(
    (f: SearchResult | null, t: SearchResult | null, chosen: Algorithm[], speed: number, autoplay = true) => {
      if (!f || !t) {
        setFormError('Pick a start and a destination station.')
        return
      }
      if (f.id === t.id) {
        setFormError('Start and destination must be different stations.')
        return
      }
      if (chosen.length === 0) {
        setFormError('Pick at least one algorithm.')
        return
      }
      setFormError(null)
      inflight.current?.abort()
      const ctl = new AbortController()
      inflight.current = ctl
      setState((prev) => ({
        status: 'loading',
        previous: prev.status === 'success' ? prev.data : prev.status === 'loading' ? prev.previous : null,
      }))
      compareAlgorithms(
        { from: f.id, to: t.id, algorithms: chosen, include_expansion: true, ...(speed > 0 ? { heuristic_speed_kmh: speed } : {}) },
        ctl.signal,
      )
        .then((d) => {
          setState({ status: 'success', data: d })
          setActive((a) => (d.results.some((r) => r.algorithm === a) ? a : (d.results[0]?.algorithm ?? 'astar')))
          if (autoplay) restart()
        })
        .catch((err: unknown) => {
          if (isAbortError(err)) return
          setState({ status: 'error', message: err instanceof ApiError ? err.message : 'Something went wrong. Please try again.' })
        })
    },
    [restart],
  )

  useEffect(() => () => inflight.current?.abort(), [])

  // Closures changed: run the same comparison again so the stats reflect them.
  const seenVersion = useRef(closures.version)
  useEffect(() => {
    if (seenVersion.current === closures.version) return
    const firstLoad = seenVersion.current === -1
    seenVersion.current = closures.version
    if (!firstLoad && data) run(from, to, algos, heuristic)
  }, [closures.version, data, from, to, algos, heuristic, run])

  function toggle(a: Algorithm) {
    setAlgos((cur) => (cur.includes(a) ? cur.filter((x) => x !== a) : ALGORITHMS.map((x) => x.id).filter((x) => x === a || cur.includes(x))))
  }

  function example() {
    const f = stationById.get(EXAMPLE.from)
    const t = stationById.get(EXAMPLE.to)
    if (!f || !t) return
    setFrom(asResult(f))
    setTo(asResult(t))
    run(asResult(f), asResult(t), algos, heuristic)
  }

  const stationActions = useCallback(
    (s: MetroStation): StationAction[] => [
      { label: 'Start here', onClick: () => setFrom(asResult(s)) },
      { label: 'Finish here', onClick: () => setTo(asResult(s)) },
    ],
    [],
  )

  const fromStation = (from && stationById.get(from.id)) || undefined
  const toStation = (to && stationById.get(to.id)) || undefined
  const color = algorithmInfo(active).color
  const fit = data ? boundsOf([...frames.map((f) => stationById.get(f.station)).filter((s): s is MetroStation => !!s)]) : null
  const bounds = fit ?? boundsOf([fromStation, toStation].filter((s): s is MetroStation => !!s))
  const fitKey = data ? `lab:${data.from.id}:${data.to.id}:${active}:${data.closures}` : `ends:${from?.id}:${to?.id}`
  const busy = state.status === 'loading'

  return (
    <div className="flex flex-col lg:grid lg:h-full lg:min-h-0 lg:grid-cols-[minmax(360px,480px)_1fr]">
      <div className="contents lg:flex lg:min-h-0 lg:flex-col lg:overflow-y-auto lg:border-r lg:border-slate-200">
        <section aria-labelledby="lab-heading" className="order-1 border-b border-slate-200 bg-white p-4">
          <h1 id="lab-heading" className="mb-1 text-base font-bold text-slate-900">
            Algorithm Lab
          </h1>
          <p className="mb-3 text-xs text-slate-600">
            Run five search algorithms on the same metro trip and watch the order in which each one explores the network.
          </p>
          <form
            className="space-y-3"
            aria-label="Compare search algorithms"
            onSubmit={(e) => {
              e.preventDefault()
              run(from, to, algos, heuristic)
            }}
          >
            <PlaceInput label="From station" placeholder="e.g. Samaypur Badli" value={from} onChange={setFrom} lineById={net?.lineById ?? NO_LINES} stationsOnly />
            <PlaceInput label="To station" placeholder="e.g. Botanical Garden" value={to} onChange={setTo} lineById={net?.lineById ?? NO_LINES} stationsOnly />
            <fieldset>
              <legend className="mb-1.5 text-xs font-semibold tracking-wide text-slate-600 uppercase">Algorithms</legend>
              <div className="flex flex-wrap gap-1.5">
                {ALGORITHMS.map((a) => {
                  const on = algos.includes(a.id)
                  return (
                    <button
                      key={a.id}
                      type="button"
                      aria-pressed={on}
                      title={a.label}
                      onClick={() => toggle(a.id)}
                      className={`inline-flex items-center gap-1.5 rounded-full border px-3 py-1.5 text-sm font-medium focus-visible:outline-2 focus-visible:outline-offset-1 focus-visible:outline-teal-600 ${
                        on ? 'border-teal-700 bg-teal-50 text-teal-900' : 'border-slate-300 bg-white text-slate-500'
                      }`}
                    >
                      <span className="size-2.5 rounded-full" style={{ background: on ? a.color : '#cbd5e1' }} aria-hidden="true" />
                      {a.short}
                    </button>
                  )
                })}
              </div>
            </fieldset>
            <label className="block text-xs font-semibold tracking-wide text-slate-600 uppercase">
              Heuristic (A* and Greedy)
              <select
                value={heuristic}
                onChange={(e) => setHeuristic(Number(e.target.value))}
                className="mt-1 block w-full rounded-lg border border-slate-300 bg-white px-3 py-2 text-sm font-normal tracking-normal text-slate-900 normal-case"
              >
                {HEURISTICS.map((h) => (
                  <option key={h.value} value={h.value}>
                    {h.label}
                  </option>
                ))}
              </select>
            </label>
            {formError && (
              <p role="alert" className="text-xs text-red-700">
                {formError}
              </p>
            )}
            <div className="flex gap-2">
              <button
                type="submit"
                disabled={busy}
                className="inline-flex flex-1 items-center justify-center gap-2 rounded-lg bg-teal-700 px-4 py-2.5 text-[15px] font-semibold text-white shadow-sm hover:bg-teal-800 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-teal-700 disabled:opacity-60"
              >
                {busy ? <LoaderCircle className="size-4 animate-spin" aria-hidden="true" /> : <Play className="size-4" aria-hidden="true" />}
                {busy ? 'Running…' : 'Run comparison'}
              </button>
            </div>
          </form>
        </section>

        <div className="order-3 space-y-3 p-4 lg:flex-1">
          <ClosurePanel closures={closures} network={net} />
          {state.status === 'error' && (
            <div role="alert" className="flex items-start gap-2 rounded-lg border border-red-200 bg-red-50 p-3 text-sm text-red-900">
              <CircleAlert className="mt-0.5 size-4 shrink-0" aria-hidden="true" />
              <div className="flex-1">
                <p>{state.message}</p>
                <button type="button" onClick={() => run(from, to, algos, heuristic)} className="mt-1 text-xs font-semibold underline">
                  Try again
                </button>
              </div>
            </div>
          )}
          {data ? (
            <div className={busy ? 'opacity-60 transition-opacity' : undefined} aria-busy={busy}>
              <LabResults
                data={data}
                active={result?.algorithm ?? active}
                onActive={(a) => {
                  setActive(a)
                  restart()
                }}
                player={player}
                total={expansion.length}
                stationsShown={visible}
              />
            </div>
          ) : (
            state.status !== 'error' && (
              <div className="rounded-lg border border-dashed border-slate-300 bg-white p-5 text-center">
                <FlaskConical className="mx-auto size-8 text-teal-700" aria-hidden="true" />
                <p className="mt-2 text-sm font-semibold text-slate-900">Compare BFS, DFS, UCS, Greedy and A*</p>
                <p className="mt-1 text-xs text-slate-600">
                  Pick two stations (or click them on the map), then run. Each algorithm's exploration plays on the map,
                  with its route cost, nodes expanded and runtime side by side.
                </p>
                <button
                  type="button"
                  onClick={example}
                  disabled={!net || busy}
                  className="mt-3 rounded-md px-3 py-1.5 text-sm font-semibold text-teal-800 hover:bg-teal-50 disabled:opacity-50"
                >
                  Try Samaypur Badli → Botanical Garden
                </button>
              </div>
            )
          )}
        </div>
      </div>

      <section aria-label="Map" className="relative order-2 h-[55vh] min-h-80 lg:order-none lg:h-full">
        <BaseMap network={net} closures={closures} stationActions={stationActions} bounds={bounds} fitKey={fitKey}>
          <ExpansionLayer
            key={`${data?.from.id}:${data?.to.id}:${active}`}
            frames={frames}
            visible={visible}
            stationById={stationById}
            color={color}
            path={finished && result?.itinerary ? result.itinerary.legs : null}
            from={data ? stationById.get(data.from.id) : fromStation}
            to={data ? stationById.get(data.to.id) : toStation}
          />
        </BaseMap>
        {data && result && (
          <div className="pointer-events-none absolute top-3 right-3 z-[1000] rounded-lg bg-white/95 px-3 py-1.5 text-xs shadow" aria-hidden="true">
            <span className="mr-1.5 inline-block size-2.5 rounded-full align-middle" style={{ background: color }} />
            <span className="font-semibold text-slate-900">{algorithmInfo(result.algorithm).short}</span>
            <span className="text-slate-600 tabular-nums">
              {' '}
              · {visible} stations · node {expansion.length ? player.step + 1 : 0}/{expansion.length}
            </span>
          </div>
        )}
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
