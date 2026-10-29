import { Ticket as TicketIcon } from 'lucide-react'
import type { Ticket } from '../api/types'
import { formatDistance, formatRupees, zoneLabel } from '../lib/format'

/** One metro ticket per operator's paid area. */
export function TicketList({ tickets }: { tickets: Ticket[] }) {
  if (tickets.length === 0) return null
  return (
    <section aria-label="Metro tickets" className="mt-4 overflow-hidden rounded-xl border border-line bg-surface">
      <h4 className="flex items-center gap-1.5 border-b border-line bg-subtle px-3 py-2 text-xs font-semibold tracking-wide text-muted uppercase">
        <TicketIcon className="size-3.5" aria-hidden="true" />
        Metro {tickets.length === 1 ? 'ticket' : 'tickets'}
      </h4>
      <ul className="divide-y divide-line">
        {tickets.map((t, i) => (
          <li key={i} className="flex items-center gap-3 px-3 py-2">
            <div className="min-w-0 flex-1">
              <p className="text-sm font-medium text-ink">{zoneLabel(t.zone)}</p>
              <p className="truncate text-xs text-muted">
                {t.from.name} → {t.to.name} · {formatDistance(t.distance_km)}
              </p>
            </div>
            <div className="text-right">
              <p className="text-sm font-semibold text-ink tabular-nums">{formatRupees(t.fare_rs)}</p>
              <p className={`text-[11px] font-medium ${t.fare_verified ? 'text-ok-ink' : 'text-warn-ink'}`}>
                {t.fare_verified ? 'Official fare' : 'Approx. fare'}
              </p>
            </div>
          </li>
        ))}
      </ul>
    </section>
  )
}
