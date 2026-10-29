import { ArrowUpDown, LoaderCircle, Search } from 'lucide-react'
import type { FormEvent } from 'react'
import type { MetroLine, Mode, SearchResult } from '../api/types'
import { modeLabel } from '../lib/format'
import { ALL_MODES, preferenceLabel, type Query } from '../lib/query'
import { ModeIcon } from './ModeIcon'
import { PlaceInput } from './PlaceInput'

interface Props {
  query: Query
  onChange: (patch: Partial<Query>, opts?: { delay?: number }) => void
  onSubmit: () => void
  busy: boolean
  lineById: Map<string, MetroLine>
}

/** Start and end fields joined by a little route line, with a swap button between them. */
export function EndpointFields({
  from,
  to,
  onChange,
  lineById,
}: {
  from: SearchResult | null
  to: SearchResult | null
  onChange: (patch: { from?: SearchResult | null; to?: SearchResult | null }) => void
  lineById: Map<string, MetroLine>
}) {
  return (
    <div role="group" aria-label="Start and destination" className="flex items-center gap-2">
      <div className="relative min-w-0 flex-1 space-y-2.5">
      <span className="pointer-events-none absolute top-6 bottom-6 left-[1.2rem] z-10 border-l-2 border-dotted border-line-strong" aria-hidden="true" />
      <PlaceInput
        label="From"
        hideLabel
        placeholder="Starting point"
        value={from}
        onChange={(v) => onChange({ from: v })}
        lineById={lineById}
        leading={<span className="block size-3 rounded-full border-[3px] border-brand bg-surface" />}
      />
      <PlaceInput
        label="To"
        hideLabel
        placeholder="Destination"
        value={to}
        onChange={(v) => onChange({ to: v })}
        lineById={lineById}
        leading={<span className="block size-3 rounded-full bg-accent ring-[3px] ring-accent/25" />}
      />
      </div>
      <button
        type="button"
        onClick={() => onChange({ from: to, to: from })}
        aria-label="Swap start and destination"
        title="Swap"
        className="grid size-10 shrink-0 place-items-center rounded-full border border-line bg-surface text-muted shadow-card transition hover:border-brand hover:text-brand-ink focus-visible:outline-2 focus-visible:outline-brand active:scale-95 pointer-coarse:size-11"
      >
        <ArrowUpDown className="size-4" aria-hidden="true" />
      </button>
    </div>
  )
}

export function JourneyForm({ query, onChange, onSubmit, busy, lineById }: Props) {
  const noModes = query.modes.length === 0
  const canSubmit = !!query.from && !!query.to && !noModes

  function toggleMode(mode: Mode) {
    const on = query.modes.includes(mode)
    const modes = on ? query.modes.filter((m) => m !== mode) : ALL_MODES.filter((m) => m === mode || query.modes.includes(m))
    onChange({ modes })
  }

  function submit(e: FormEvent) {
    e.preventDefault()
    if (canSubmit) onSubmit()
  }

  return (
    <form onSubmit={submit} aria-label="Plan a journey" className="space-y-5">
      <EndpointFields from={query.from} to={query.to} onChange={onChange} lineById={lineById} />

      <fieldset>
        <legend className="mb-2 text-xs font-semibold tracking-wide text-muted uppercase">Travel by</legend>
        <div className="-mx-1 flex gap-1.5 overflow-x-auto px-1 pb-1 [scrollbar-width:none]">
          {ALL_MODES.map((mode) => {
            const on = query.modes.includes(mode)
            return (
              <button
                key={mode}
                type="button"
                aria-pressed={on}
                onClick={() => toggleMode(mode)}
                className={`inline-flex h-10 shrink-0 items-center gap-1.5 rounded-full border px-3.5 text-sm font-medium transition-colors focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-brand pointer-coarse:h-11 ${
                  on
                    ? 'border-transparent bg-brand-soft text-brand-ink'
                    : 'border-line bg-surface text-muted hover:border-line-strong hover:text-ink'
                }`}
              >
                <ModeIcon mode={mode} className="size-4" />
                {modeLabel(mode)}
              </button>
            )
          })}
        </div>
        {noModes && (
          <p role="alert" className="mt-1.5 text-xs text-danger-ink">
            Pick at least one way to travel.
          </p>
        )}
      </fieldset>

      <div className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-1 xl:grid-cols-2">
        <div>
          <label htmlFor="depart-at" className="mb-2 block text-xs font-semibold tracking-wide text-muted uppercase">
            Leave at
          </label>
          <input
            id="depart-at"
            type="datetime-local"
            required
            value={query.departAt}
            onChange={(e) => e.target.value && onChange({ departAt: e.target.value }, { delay: 500 })}
            className="h-11 w-full rounded-control border border-line-strong bg-surface px-3 text-[15px] text-ink focus:border-brand focus:ring-4 focus:ring-brand/15 focus:outline-none"
          />
        </div>
        <div>
          <div className="mb-2 flex items-baseline justify-between">
            <label htmlFor="preference" className="text-xs font-semibold tracking-wide text-muted uppercase">
              Preference
            </label>
            <span className="text-xs font-semibold text-brand-ink" aria-hidden="true">
              {preferenceLabel(query.preference)}
            </span>
          </div>
          <input
            id="preference"
            type="range"
            min={0}
            max={1}
            step={0.05}
            value={query.preference}
            aria-valuetext={preferenceLabel(query.preference)}
            onChange={(e) => onChange({ preference: Number(e.target.value) }, { delay: 400 })}
            className="h-6 w-full accent-brand"
          />
          <div className="flex justify-between text-xs text-muted" aria-hidden="true">
            <span>Cheapest</span>
            <span>Fastest</span>
          </div>
        </div>
      </div>

      <button
        type="submit"
        disabled={!canSubmit || busy}
        className="inline-flex h-12 w-full items-center justify-center gap-2 rounded-control bg-brand px-4 text-base font-semibold text-on-brand shadow-card transition hover:bg-brand-strong focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-brand active:scale-[0.99] disabled:cursor-not-allowed disabled:bg-subtle-2 disabled:text-muted disabled:shadow-none"
      >
        {busy ? <LoaderCircle className="size-4 animate-spin" aria-hidden="true" /> : <Search className="size-4" aria-hidden="true" />}
        {busy ? 'Finding routes…' : 'Find routes'}
      </button>
    </form>
  )
}
