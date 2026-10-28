import { useState } from 'react'
import { barPercent } from '../lib/lab'

export interface BarRow {
  key: string
  label: string
  value: number | null // null = no value (e.g. no route found)
  valueLabel: string
  color: string
  detail?: string
}

interface Props {
  title: string
  note?: string
  rows: readonly BarRow[]
}

/** Horizontal bars with the value written next to each bar, so colour is never the only cue. */
export function BarChart({ title, note, rows }: Props) {
  const [hover, setHover] = useState<string | null>(null)
  const max = Math.max(0, ...rows.map((r) => r.value ?? 0))
  return (
    <figure className="min-w-0">
      <figcaption className="mb-1.5">
        <span className="text-xs font-semibold text-slate-800">{title}</span>
        {note && <span className="ml-1.5 text-[11px] text-slate-500">{note}</span>}
      </figcaption>
      <ul className="space-y-1">
        {rows.map((r) => (
          <li
            key={r.key}
            className="relative grid grid-cols-[3.25rem_1fr] items-center gap-2"
            onMouseEnter={() => setHover(r.key)}
            onMouseLeave={() => setHover((h) => (h === r.key ? null : h))}
          >
            <span className="truncate text-[11px] font-medium text-slate-700">{r.label}</span>
            <span className="flex min-w-0 items-center gap-1.5">
              <span className="h-3 min-w-0 flex-1" aria-hidden="true">
                <span
                  className="block h-full rounded-r transition-[width] duration-300"
                  style={{
                    width: `${Math.max(r.value ? 1.5 : 0, barPercent(r.value ?? 0, max))}%`,
                    background: r.color,
                    opacity: hover && hover !== r.key ? 0.45 : 1,
                  }}
                />
              </span>
              <span className="w-16 shrink-0 text-right text-[11px] tabular-nums text-slate-800">{r.valueLabel}</span>
            </span>
            {hover === r.key && r.detail && (
              <span
                role="tooltip"
                className="pointer-events-none absolute -top-7 left-14 z-10 rounded bg-slate-900 px-2 py-1 text-[11px] whitespace-nowrap text-white shadow"
              >
                {r.detail}
              </span>
            )}
          </li>
        ))}
      </ul>
    </figure>
  )
}
