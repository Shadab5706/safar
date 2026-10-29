import { CircleCheck, Pause, Play, RotateCcw, SkipBack, SkipForward, StepBack, StepForward } from 'lucide-react'
import type { ReactNode } from 'react'
import type { Algorithm, LabResponse } from '../api/types'
import { SPEEDS, type Player } from '../hooks/usePlayer'
import { algorithmInfo, bestOf } from '../lib/lab'
import { BarChart, type BarRow } from './BarChart'

interface Props {
  data: LabResponse
  active: Algorithm
  onActive: (a: Algorithm) => void
  player: Player
  total: number // raw expansion length of the active algorithm
  stationsShown: number
}

const fmtCost = (v: number) => `${v.toFixed(1)} min`
const fmtGap = (v: number | undefined) => (v === undefined ? '–' : v === 0 ? 'optimal' : `+${v.toFixed(1)}%`)
const fmtUs = (v: number) => (v >= 1000 ? `${(v / 1000).toFixed(2)} ms` : `${Math.round(v)} µs`)

function IconButton({ label, onClick, children, disabled }: { label: string; onClick: () => void; children: ReactNode; disabled?: boolean }) {
  return (
    <button
      type="button"
      aria-label={label}
      title={label}
      disabled={disabled}
      onClick={onClick}
      className="inline-flex size-9 items-center justify-center rounded-md text-ink-2 hover:bg-subtle focus-visible:outline-2 focus-visible:outline-brand disabled:opacity-40"
    >
      {children}
    </button>
  )
}

export function LabResults({ data, active, onActive, player, total, stationsShown }: Props) {
  const results = data.results
  const current = results.find((r) => r.algorithm === active)
  const info = algorithmInfo(active)
  const lastStep = Math.max(0, total - 1)
  const finished = total > 0 && player.step >= lastStep
  const bestExpanded = bestOf(results, (r) => r.stats.nodes_expanded)

  const rows = (pick: (r: (typeof results)[number]) => number | null, label: (v: number) => string, detail: (r: (typeof results)[number]) => string): BarRow[] =>
    results.map((r) => {
      const a = algorithmInfo(r.algorithm)
      const v = pick(r)
      return { key: r.algorithm, label: a.short, value: v, valueLabel: v === null ? 'no route' : label(v), color: a.color, detail: detail(r) }
    })

  return (
    <div className="space-y-4">
      <section aria-labelledby="lab-player" className="rounded-lg border border-line bg-surface p-3">
        <h2 id="lab-player" className="sr-only">
          Search animation
        </h2>
        <div role="tablist" aria-label="Algorithm to animate" className="flex flex-wrap gap-1">
          {results.map((r) => {
            const a = algorithmInfo(r.algorithm)
            const selected = r.algorithm === active
            return (
              <button
                key={r.algorithm}
                type="button"
                role="tab"
                aria-selected={selected}
                onClick={() => onActive(r.algorithm)}
                className={`inline-flex items-center gap-1.5 rounded-full border px-3 py-1 text-sm font-medium focus-visible:outline-2 focus-visible:outline-offset-1 focus-visible:outline-brand ${
                  selected ? 'border-ink bg-ink text-on-brand' : 'border-line-strong bg-surface text-ink-2 hover:bg-subtle'
                }`}
              >
                <span className="size-2.5 rounded-full" style={{ background: a.color }} aria-hidden="true" />
                {a.short}
              </button>
            )
          })}
        </div>

        <p className="mt-2 text-xs text-muted">
          <span className="font-semibold text-ink-2">{info.label}:</span> {info.blurb.toLowerCase()}.
        </p>

        <div className="mt-2 flex flex-wrap items-center gap-0.5">
          <IconButton label="Back to start" onClick={() => player.seek(0)}>
            <SkipBack className="size-4" aria-hidden="true" />
          </IconButton>
          <IconButton label="Step back" onClick={() => player.seek(player.step - 1)} disabled={player.step <= 0}>
            <StepBack className="size-4" aria-hidden="true" />
          </IconButton>
          {player.playing ? (
            <IconButton label="Pause" onClick={player.pause}>
              <Pause className="size-5" aria-hidden="true" />
            </IconButton>
          ) : (
            <IconButton label={finished ? 'Replay' : 'Play'} onClick={finished ? player.restart : player.play} disabled={total === 0}>
              {finished ? <RotateCcw className="size-4" aria-hidden="true" /> : <Play className="size-5" aria-hidden="true" />}
            </IconButton>
          )}
          <IconButton label="Step forward" onClick={() => player.seek(player.step + 1)} disabled={player.step >= lastStep}>
            <StepForward className="size-4" aria-hidden="true" />
          </IconButton>
          <IconButton label="Skip to end" onClick={() => player.seek(lastStep)} disabled={player.step >= lastStep}>
            <SkipForward className="size-4" aria-hidden="true" />
          </IconButton>
          <label className="ml-auto flex items-center gap-1.5 text-xs text-muted">
            Speed
            <select
              value={player.rate}
              onChange={(e) => player.setRate(Number(e.target.value))}
              className="rounded-md border border-line-strong bg-surface px-1.5 py-1 text-xs text-ink"
            >
              {SPEEDS.map((s) => (
                <option key={s.rate} value={s.rate}>
                  {s.label} ({s.rate}/s)
                </option>
              ))}
            </select>
          </label>
        </div>
        <input
          type="range"
          min={0}
          max={lastStep}
          value={player.step}
          onChange={(e) => player.seek(Number(e.target.value))}
          aria-label="Animation progress"
          aria-valuetext={`Node ${player.step + 1} of ${total} expanded`}
          disabled={total === 0}
          className="mt-1 w-full"
          style={{ accentColor: info.color }}
        />
        <p className="text-xs text-muted tabular-nums">
          Node {total === 0 ? 0 : player.step + 1} of {total} expanded · {stationsShown} stations reached
        </p>
        <p role="status" className="mt-1 min-h-5 text-xs font-medium">
          {finished &&
            (current?.found ? (
              <span className="inline-flex items-center gap-1 text-brand-ink">
                <CircleCheck className="size-3.5" aria-hidden="true" />
                Goal reached: {fmtCost(current.cost)}
                {current.optimality_gap_pct ? ` (${fmtGap(current.optimality_gap_pct)} longer than the best route)` : ' (the best route)'}
              </span>
            ) : (
              <span className="text-danger-ink">Searched everything reachable: no route (check the line closures).</span>
            ))}
        </p>
      </section>

      <section aria-labelledby="lab-stats">
        <h2 id="lab-stats" className="mb-2 text-sm font-bold text-ink">
          {data.from.name} → {data.to.name}
        </h2>
        <div className="overflow-x-auto rounded-lg border border-line bg-surface">
          <table className="w-full min-w-[440px] text-left text-xs">
            <caption className="sr-only">Search statistics per algorithm</caption>
            <thead className="bg-subtle text-[11px] tracking-wide text-muted uppercase">
              <tr>
                <th scope="col" className="px-2 py-1.5 font-semibold">Algorithm</th>
                <th scope="col" className="px-2 py-1.5 text-right font-semibold">Cost</th>
                <th scope="col" className="px-2 py-1.5 text-right font-semibold">Gap</th>
                <th scope="col" className="px-2 py-1.5 text-right font-semibold">Expanded</th>
                <th scope="col" className="px-2 py-1.5 text-right font-semibold">Frontier</th>
                <th scope="col" className="px-2 py-1.5 text-right font-semibold">Time</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-line tabular-nums">
              {results.map((r) => {
                const a = algorithmInfo(r.algorithm)
                return (
                  <tr key={r.algorithm} className={r.algorithm === active ? 'bg-subtle' : undefined}>
                    <th scope="row" className="px-2 py-1.5 font-medium text-ink">
                      <span className="inline-flex items-center gap-1.5">
                        <span className="size-2.5 rounded-full" style={{ background: a.color }} aria-hidden="true" />
                        {a.short}
                      </span>
                    </th>
                    <td className="px-2 py-1.5 text-right">{r.found ? fmtCost(r.cost) : 'no route'}</td>
                    <td className={`px-2 py-1.5 text-right ${r.optimality_gap_pct === 0 ? 'font-semibold text-brand-ink' : ''}`}>
                      {r.found ? fmtGap(r.optimality_gap_pct) : '–'}
                    </td>
                    <td className={`px-2 py-1.5 text-right ${r.stats.nodes_expanded === bestExpanded ? 'font-semibold' : ''}`}>
                      {r.stats.nodes_expanded}
                    </td>
                    <td className="px-2 py-1.5 text-right">{r.stats.max_frontier}</td>
                    <td className="px-2 py-1.5 text-right">{fmtUs(r.stats.runtime_us)}</td>
                  </tr>
                )
              })}
            </tbody>
          </table>
        </div>
        <p className="mt-1.5 text-[11px] text-muted">
          Cost = minutes door to door on the metro (boarding, waiting, riding, changing). Gap is against the uniform-cost
          optimum{data.optimal_cost !== null ? ` of ${fmtCost(data.optimal_cost)}` : ''}. Heuristic speed{' '}
          {Math.round(data.heuristic_speed_kmh)} km/h. Runtimes vary run to run.
        </p>
      </section>

      <section aria-label="Charts" className="grid grid-cols-1 gap-4 rounded-lg border border-line bg-surface p-3 sm:grid-cols-2 lg:grid-cols-1 xl:grid-cols-2">
        <BarChart
          title="Nodes expanded"
          note="fewer = less work"
          rows={rows(
            (r) => r.stats.nodes_expanded,
            (v) => String(v),
            (r) => `${r.stats.nodes_generated} generated, frontier peak ${r.stats.max_frontier}`,
          )}
        />
        <BarChart
          title="Path cost"
          note="lower = better route"
          rows={rows(
            (r) => (r.found ? r.cost : null),
            fmtCost,
            (r) => (r.found ? `gap ${fmtGap(r.optimality_gap_pct)}` : 'no route'),
          )}
        />
        <BarChart
          title="Runtime"
          note="this run, on the server"
          rows={rows(
            (r) => r.stats.runtime_us,
            fmtUs,
            (r) => `${r.stats.nodes_expanded} nodes in ${fmtUs(r.stats.runtime_us)}`,
          )}
        />
      </section>
    </div>
  )
}
