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
    <div className="mt-2 rounded-lg border border-slate-200 bg-slate-50/70">
      <p className="border-b border-slate-200 px-3 py-1.5 text-xs text-slate-600">
        <span className="font-semibold text-amber-800">Estimated prices.</span> Open the app for the live fare.
      </p>
      <ul className="divide-y divide-slate-200">
        {quotes.map((q) => (
          <li key={`${q.provider}:${q.product}`} className="px-3 py-2">
            <div className="flex items-baseline justify-between gap-3">
              <p className="min-w-0 text-sm font-medium text-slate-900">
                {q.product_name}
                {q.mode !== mode && (
                  <span className="ml-1.5 rounded bg-slate-200 px-1 py-px text-[11px] font-medium whitespace-nowrap text-slate-600">
                    {modeLabel(q.mode)}
                  </span>
                )}
              </p>
              <p className="shrink-0 text-sm font-semibold text-slate-900 tabular-nums">
                {formatRupees(q.fare_rs)}
                <span className="ml-1 text-[11px] font-medium text-amber-800">est.</span>
              </p>
            </div>
            <div className="mt-0.5 flex items-center justify-between gap-3">
              <p className="min-w-0 text-xs text-slate-500">
                {formatRupeeRange(q.fare_range_rs)}
                {' · '}
                {q.deep_link ? `pickup ~${formatDuration(q.pickup_eta_min)}` : 'hail on the street, pay by meter'}
                {q.surge > 1.01 && ` · ×${q.surge.toFixed(2)} demand`}
              </p>
              {q.deep_link ? (
                <a
                  href={q.deep_link.url}
                  target="_blank"
                  rel="noopener noreferrer"
                  aria-label={`${linkLabel(q)}: ${q.product_name}${q.deep_link.prefilled ? ' with pickup and drop filled in' : ''} (opens in a new tab)`}
                  title={q.deep_link.documented ? undefined : 'This link format is not officially documented by the provider'}
                  className="inline-flex shrink-0 items-center gap-1 rounded-md border border-slate-300 bg-white px-2 py-0.5 text-xs font-medium text-teal-800 hover:border-teal-600 hover:bg-teal-50 focus-visible:outline-2 focus-visible:outline-teal-600"
                >
                  {linkLabel(q)}
                  <ExternalLink className="size-3" aria-hidden="true" />
                </a>
              ) : (
                <span className="shrink-0 text-xs text-slate-500">Govt meter</span>
              )}
            </div>
          </li>
        ))}
      </ul>
    </div>
  )
}
