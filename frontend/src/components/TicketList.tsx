import { Ticket as TicketIcon } from 'lucide-react'
import type { Ticket } from '../api/types'
import { formatDistance, formatRupees, zoneLabel } from '../lib/format'

/** One metro ticket per operator's paid area. */
export function TicketList({ tickets }: { tickets: Ticket[] }) {
  if (tickets.length === 0) return null
  return (
    <section aria-label="Metro tickets" className="mt-3 rounded-lg border border-slate-200">
      <h4 className="flex items-center gap-1.5 border-b border-slate-200 px-3 py-1.5 text-xs font-semibold tracking-wide text-slate-600 uppercase">
        <TicketIcon className="size-3.5" aria-hidden="true" />
        Metro {tickets.length === 1 ? 'ticket' : 'tickets'}
      </h4>
      <ul className="divide-y divide-slate-200">
        {tickets.map((t, i) => (
          <li key={i} className="flex items-center gap-3 px-3 py-2">
            <div className="min-w-0 flex-1">
              <p className="text-sm font-medium text-slate-900">{zoneLabel(t.zone)}</p>
              <p className="truncate text-xs text-slate-500">
                {t.from.name} → {t.to.name} · {formatDistance(t.distance_km)}
              </p>
            </div>
            <div className="text-right">
              <p className="text-sm font-semibold text-slate-900 tabular-nums">{formatRupees(t.fare_rs)}</p>
              <p className={`text-[11px] font-medium ${t.fare_verified ? 'text-emerald-700' : 'text-amber-800'}`}>
                {t.fare_verified ? 'Official fare' : 'Approx. fare'}
              </p>
            </div>
          </li>
        ))}
      </ul>
    </section>
  )
}
