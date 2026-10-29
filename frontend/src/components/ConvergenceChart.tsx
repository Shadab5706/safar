import { useId, useState, type MouseEvent } from 'react'
import type { TripResult } from '../api/types'
import { linePath, linearScale, methodInfo, niceTicks, pointAt } from '../lib/trip'

interface Props {
  results: readonly TripResult[]
  optimum: number | null
}

const W = 420
const H = 230
const M = { top: 14, right: 14, bottom: 34, left: 48 }

const fmt = (v: number) => (Math.abs(v) >= 1000 ? `${(v / 1000).toFixed(1)}k` : String(Math.round(v)))

/** Tour cost against tours evaluated, for each method: best-so-far (solid) and the current tour (faint). */
export function ConvergenceChart({ results, optimum }: Props) {
  const id = useId()
  const [hoverX, setHoverX] = useState<number | null>(null)
  const points = results.flatMap((r) => r.curve)
  if (points.length === 0) return null

  const maxX = Math.max(...points.map((p) => p.evaluations))
  const values = points.flatMap((p) => [p.current, p.best]).concat(optimum ?? [])
  const lo = Math.min(...values)
  const hi = Math.max(...values)
  const pad = (hi - lo) * 0.06 || 1
  const x = linearScale([0, maxX], [M.left, W - M.right])
  const y = linearScale([lo - pad, hi + pad], [H - M.bottom, M.top])
  const xTicks = niceTicks(0, maxX, 4)
  const yTicks = niceTicks(lo - pad, hi + pad, 4)
  const plotW = W - M.left - M.right

  function onMove(e: MouseEvent<SVGRectElement>) {
    const rect = e.currentTarget.getBoundingClientRect()
    const share = (e.clientX - rect.left) / rect.width
    setHoverX(Math.max(0, Math.min(maxX, share * maxX)))
  }

  const hover = hoverX === null ? null : results.map((r) => ({ r, p: pointAt(r.curve, hoverX) }))
  const hoverPx = hoverX === null ? 0 : x(hoverX)

  return (
    <figure className="min-w-0">
      <figcaption id={`${id}-cap`} className="mb-1 text-xs font-semibold text-ink-2">
        Convergence: tour cost vs tours evaluated
      </figcaption>
      <ul className="mb-1.5 flex flex-wrap gap-x-3 gap-y-1 text-[11px] text-ink-2">
        {results.map((r) => (
          <li key={r.method} className="flex items-center gap-1.5">
            <span className="inline-block h-0.5 w-4 rounded-full" style={{ background: methodInfo(r.method).color }} aria-hidden="true" />
            {methodInfo(r.method).label}
          </li>
        ))}
        <li className="flex items-center gap-1.5 text-muted">solid = best so far, faint = current tour</li>
        {optimum !== null && (
          <li className="flex items-center gap-1.5 text-muted">
            <span className="inline-block w-4 border-t-2 border-dashed border-faint" aria-hidden="true" />
            exact optimum
          </li>
        )}
      </ul>
      <div className="relative">
        <svg
          viewBox={`0 0 ${W} ${H}`}
          className="h-auto w-full"
          role="img"
          aria-labelledby={`${id}-cap`}
          aria-describedby={`${id}-desc`}
        >
          <desc id={`${id}-desc`}>
            {results
              .map((r) => `${methodInfo(r.method).label} ends at cost ${r.score} after ${r.evaluations} tours`)
              .join('; ')}
            {optimum !== null ? `; the exact optimum is ${optimum}.` : '.'}
          </desc>
          {yTicks.map((t) => (
            <g key={`y${t}`}>
              <line x1={M.left} x2={W - M.right} y1={y(t)} y2={y(t)} stroke="var(--line)" strokeWidth={1} />
              <text x={M.left - 6} y={y(t)} dy="0.32em" textAnchor="end" fontSize={10} fill="var(--muted)">
                {fmt(t)}
              </text>
            </g>
          ))}
          {xTicks.map((t) => (
            <text key={`x${t}`} x={x(t)} y={H - M.bottom + 14} textAnchor="middle" fontSize={10} fill="var(--muted)">
              {fmt(t)}
            </text>
          ))}
          <line x1={M.left} x2={W - M.right} y1={H - M.bottom} y2={H - M.bottom} stroke="var(--line-strong)" strokeWidth={1} />
          <text x={M.left + plotW / 2} y={H - 4} textAnchor="middle" fontSize={10} fill="var(--ink-2)">
            tours evaluated
          </text>
          <text transform={`translate(11 ${M.top + (H - M.top - M.bottom) / 2}) rotate(-90)`} textAnchor="middle" fontSize={10} fill="var(--ink-2)">
            cost (min-equivalent)
          </text>

          {optimum !== null && (
            <line x1={M.left} x2={W - M.right} y1={y(optimum)} y2={y(optimum)} stroke="var(--muted)" strokeWidth={1.5} strokeDasharray="5 4" />
          )}
          {results.map((r) => (
            <path key={`c-${r.method}`} d={linePath(r.curve, 'current', x, y)} fill="none" stroke={methodInfo(r.method).color} strokeWidth={1.25} strokeOpacity={0.35} />
          ))}
          {results.map((r) => (
            <path key={`b-${r.method}`} d={linePath(r.curve, 'best', x, y, true)} fill="none" stroke={methodInfo(r.method).color} strokeWidth={2} strokeLinejoin="round" />
          ))}

          {hover && (
            <g pointerEvents="none">
              <line x1={hoverPx} x2={hoverPx} y1={M.top} y2={H - M.bottom} stroke="var(--muted)" strokeWidth={1} strokeDasharray="2 3" />
              {hover.map(({ r, p }) =>
                p ? <circle key={r.method} cx={hoverPx} cy={y(p.best)} r={4} fill={methodInfo(r.method).color} stroke="var(--surface)" strokeWidth={2} /> : null,
              )}
            </g>
          )}
          <rect
            x={M.left}
            y={M.top}
            width={plotW}
            height={H - M.top - M.bottom}
            fill="transparent"
            onMouseMove={onMove}
            onMouseLeave={() => setHoverX(null)}
          />
        </svg>
        {hover && hoverX !== null && (
          <div
            role="tooltip"
            className="pointer-events-none absolute top-2 z-10 rounded-md bg-ink px-2.5 py-1.5 text-[11px] text-on-brand shadow"
            style={hoverPx > W / 2 ? { right: `${((W - hoverPx) / W) * 100 + 2}%` } : { left: `${(hoverPx / W) * 100 + 2}%` }}
          >
            <p className="font-semibold tabular-nums">{Math.round(hoverX)} tours</p>
            {hover.map(({ r, p }) =>
              p ? (
                <p key={r.method} className="tabular-nums">
                  <span className="mr-1 inline-block size-2 rounded-full" style={{ background: methodInfo(r.method).color }} />
                  {methodInfo(r.method).label}: best {p.best.toFixed(1)}, current {p.current.toFixed(1)}
                  {p.temperature !== undefined ? `, T ${p.temperature.toFixed(1)}` : ''}
                </p>
              ) : null,
            )}
          </div>
        )}
      </div>
    </figure>
  )
}
