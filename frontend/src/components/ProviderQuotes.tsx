import { ExternalLink } from 'lucide-react'
import type { FareQuote, RoadMode } from '../api/types'
import { formatDuration, formatRupeeRange, formatRupees, modeLabel } from '../lib/format'
import { sortQuotes } from '../lib/route'

function linkLabel(q: FareQuote): string {
  if (!q.deep_link) return ''
  return q.deep_link.prefilled ? 'Open in app' : 'Open site'
}

export function ProviderQuotes({ options, mode }: { options: FareQuote[]; mode: RoadMode }) {
  const quotes = sortQuotes(options, mode)
  if (quotes.length === 0) return null
  return (
    <div className="mt-2.5 overflow-hidden rounded-xl border border-line bg-surface">
      <p className="border-b border-line bg-subtle px-3 py-2 text-xs text-muted">
        <span className="font-semibold text-ink-2">Fair prices</span> from real trip fares. If the app asks for more, it&apos;s
        surge or overpricing.
      </p>
      <ul className="divide-y divide-line">
        {quotes.map((q) => (
          <li key={`${q.provider}:${q.product}`} className="px-3 py-2.5">
            <div className="flex items-baseline justify-between gap-3">
              <p className="min-w-0 text-sm font-medium text-ink">
                {q.product_name}
                {q.mode !== mode && (
                  <span className="ml-1.5 rounded-md bg-subtle-2 px-1.5 py-px text-[11px] font-medium whitespace-nowrap text-muted">
                    {modeLabel(q.mode)}
                  </span>
                )}
              </p>
              <p className="shrink-0 text-sm font-semibold text-ink tabular-nums">{formatRupeeRange(q.fare_range_rs)}</p>
            </div>
            <div className="mt-0.5 flex items-center justify-between gap-3">
              <p className="min-w-0 text-xs text-muted">
                Pay up to {formatRupees(q.fare_range_rs[1])}
                {' · '}
                {q.deep_link ? `pickup ~${formatDuration(q.pickup_eta_min)}` : 'hail on the street, pay by meter'}
                {q.surge > 1.01 && ` · ×${q.surge.toFixed(2)} demand`}
                {(q.airport_pickup_rs ?? 0) > 0 && ` · incl. ${formatRupees(q.airport_pickup_rs!)} airport pickup`}
              </p>
              {q.deep_link ? (
                <a
                  href={q.deep_link.url}
                  target="_blank"
                  rel="noopener noreferrer"
                  aria-label={`${linkLabel(q)}: ${q.product_name}${q.deep_link.prefilled ? ' with pickup and drop filled in' : ''} (opens in a new tab)`}
                  title={q.deep_link.documented ? undefined : 'This link format is not officially documented by the provider'}
                  className="inline-flex h-8 shrink-0 items-center gap-1 rounded-full bg-brand-soft px-3 text-xs font-semibold text-brand-ink transition-colors hover:bg-brand hover:text-on-brand focus-visible:outline-2 focus-visible:outline-brand pointer-coarse:h-11"
                >
                  {linkLabel(q)}
                  <ExternalLink className="size-3" aria-hidden="true" />
                </a>
              ) : (
                <span className="shrink-0 text-xs text-muted">Govt meter</span>
              )}
            </div>
          </li>
        ))}
      </ul>
    </div>
  )
}
