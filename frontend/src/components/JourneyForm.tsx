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
    <form onSubmit={submit} aria-label="Plan a journey" className="space-y-4">
      <div className="relative space-y-3">
        <PlaceInput
          label="From"
          placeholder="Bennett University, a metro station…"
          value={query.from}
          onChange={(from: SearchResult | null) => onChange({ from })}
          lineById={lineById}
        />
        <button
          type="button"
          onClick={() => onChange({ from: query.to, to: query.from })}
          aria-label="Swap from and to"
          className="absolute top-[72px] right-0 z-10 inline-flex items-center gap-1 rounded px-1.5 py-0.5 text-xs font-medium text-teal-800 hover:bg-teal-50 focus-visible:outline-2 focus-visible:outline-teal-600"
        >
          <ArrowUpDown className="size-3.5" aria-hidden="true" />
          Swap
        </button>
        <PlaceInput
          label="To"
          placeholder="Hauz Khas Village, Cyber Hub…"
          value={query.to}
          onChange={(to: SearchResult | null) => onChange({ to })}
          lineById={lineById}
        />
      </div>

      <div>
        <label htmlFor="depart-at" className="mb-1 block text-xs font-semibold tracking-wide text-slate-600 uppercase">
          Depart at
        </label>
        <input
          id="depart-at"
          type="datetime-local"
          required
          value={query.departAt}
          onChange={(e) => e.target.value && onChange({ departAt: e.target.value }, { delay: 500 })}
          className="w-full rounded-lg border border-slate-300 bg-white px-3 py-2 text-[15px] text-slate-900 shadow-sm focus:border-teal-600 focus:ring-2 focus:ring-teal-600/20 focus:outline-none"
        />
      </div>

      <fieldset>
        <legend className="mb-1.5 text-xs font-semibold tracking-wide text-slate-600 uppercase">Travel by</legend>
        <div className="flex flex-wrap gap-1.5">
          {ALL_MODES.map((mode) => {
            const on = query.modes.includes(mode)
            return (
              <button
                key={mode}
                type="button"
                aria-pressed={on}
                onClick={() => toggleMode(mode)}
                className={`inline-flex items-center gap-1.5 rounded-full border px-3 py-1.5 text-sm font-medium transition-colors focus-visible:outline-2 focus-visible:outline-offset-1 focus-visible:outline-teal-600 ${
                  on
                    ? 'border-teal-700 bg-teal-700 text-white hover:bg-teal-800'
                    : 'border-slate-300 bg-white text-slate-600 hover:border-slate-400 hover:text-slate-900'
                }`}
              >
                <ModeIcon mode={mode} className="size-4" />
                {modeLabel(mode)}
              </button>
            )
          })}
        </div>
        {noModes && (
          <p role="alert" className="mt-1.5 text-xs text-red-700">
            Pick at least one way to travel.
          </p>
        )}
      </fieldset>

      <div>
        <div className="mb-1 flex items-baseline justify-between">
          <label htmlFor="preference" className="text-xs font-semibold tracking-wide text-slate-600 uppercase">
            Preference
          </label>
          <span className="text-xs font-medium text-teal-800" aria-hidden="true">
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
          className="w-full accent-teal-700"
        />
        <div className="flex justify-between text-xs text-slate-500" aria-hidden="true">
          <span>Cheapest</span>
          <span>Fastest</span>
        </div>
      </div>

      <button
        type="submit"
        disabled={!canSubmit || busy}
        className="inline-flex w-full items-center justify-center gap-2 rounded-lg bg-teal-700 px-4 py-2.5 text-[15px] font-semibold text-white shadow-sm hover:bg-teal-800 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-teal-700 disabled:cursor-not-allowed disabled:bg-slate-300 disabled:text-slate-600"
      >
        {busy ? (
          <LoaderCircle className="size-4 animate-spin" aria-hidden="true" />
        ) : (
          <Search className="size-4" aria-hidden="true" />
        )}
        {busy ? 'Finding routes…' : 'Find routes'}
      </button>
    </form>
  )
}
