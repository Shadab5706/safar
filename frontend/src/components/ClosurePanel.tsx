import { ChevronDown, CircleAlert, Construction, X } from 'lucide-react'
import { useId, useMemo, useState } from 'react'
import type { ClosuresApi } from '../hooks/useClosures'
import type { MetroNetwork } from '../hooks/useMetroNetwork'
import { CLOSED_COLOR, networkSegments } from '../lib/closures'

interface Props {
  closures: ClosuresApi
  network: MetroNetwork | null
}

/** List of closed stations / sections, with a keyboard-friendly way to add one. */
export function ClosurePanel({ closures, network }: Props) {
  const id = useId()
  const [open, setOpen] = useState(false)
  const [lineId, setLineId] = useState('')
  const [target, setTarget] = useState('')
  const count = closures.items.length

  const lines = network?.lines ?? []
  const line = network?.lineById.get(lineId)
  const choices = useMemo(() => {
    if (!network || !line) return []
    const byId = new Map(network.stations.map((s) => [s.id, s]))
    const stations = [...new Set(line.stations)].flatMap((sid) => {
      const s = byId.get(sid)
      return s ? [{ value: `station|${s.id}`, label: `Station: ${s.name}` }] : []
    })
    const sections = networkSegments([line], byId).map((seg) => ({
      value: `segment|${seg.a.id}|${seg.b.id}`,
      label: `Section: ${seg.a.name} – ${seg.b.name}`,
    }))
    return [...stations, ...sections]
  }, [network, line])

  function submit() {
    const [kind, a, b] = target.split('|')
    if (kind === 'station' && a) closures.close({ type: 'station', station: a })
    else if (kind === 'segment' && a && b && line) closures.close({ type: 'segment', line: line.id, from: a, to: b })
    setTarget('')
  }

  return (
    <div className="rounded-control border border-line bg-surface">
      <button
        type="button"
        aria-expanded={open}
        aria-controls={`${id}-body`}
        onClick={() => setOpen((o) => !o)}
        className="flex min-h-11 w-full items-center gap-2 rounded-control px-3.5 py-2 text-left text-sm font-semibold text-ink-2 transition-colors hover:bg-subtle focus-visible:outline-2 focus-visible:outline-brand"
      >
        <Construction className="size-4 text-muted" aria-hidden="true" />
        Line closures
        {count > 0 ? (
          <span className="rounded-full bg-danger-soft px-2 py-0.5 text-xs font-bold text-danger-ink">{count} active</span>
        ) : (
          <span className="text-xs font-normal text-muted">none</span>
        )}
        <ChevronDown className={`ml-auto size-4 text-muted transition-transform ${open ? 'rotate-180' : ''}`} aria-hidden="true" />
      </button>

      {closures.error && (
        <div role="alert" className="mx-3 mb-2 flex items-start gap-2 rounded-md bg-danger-soft px-2.5 py-2 text-xs text-danger-ink">
          <CircleAlert className="mt-px size-4 shrink-0" aria-hidden="true" />
          <span className="flex-1">{closures.error}</span>
          {closures.status === 'error' ? (
            <button type="button" onClick={closures.reload} className="font-semibold underline">
              Retry
            </button>
          ) : (
            <button type="button" onClick={closures.dismissError} aria-label="Dismiss" className="rounded p-0.5 hover:bg-danger-soft">
              <X className="size-3.5" aria-hidden="true" />
            </button>
          )}
        </div>
      )}

      <div id={`${id}-body`} hidden={!open} className="space-y-3 border-t border-line px-3 pt-2 pb-3">
        <p className="text-xs text-muted">
          Click a station or a line on the map to close it. A closed station is skipped (trains run through without
          stopping); a closed section cuts the line. Every route re-plans around them.
        </p>

        {count > 0 && (
          <ul className="space-y-1.5" aria-label="Active closures">
            {closures.items.map((c) => (
              <li key={c.id} className="flex items-center gap-2 text-xs text-ink-2">
                <span
                  className="inline-block h-1.5 w-4 shrink-0 rounded-full"
                  style={{ background: c.type === 'segment' ? c.line.color : CLOSED_COLOR }}
                  aria-hidden="true"
                />
                <span className="flex-1">{c.label}</span>
                <button
                  type="button"
                  disabled={closures.pending}
                  onClick={() => closures.reopen(c.id)}
                  className="rounded-md px-2 py-1 font-semibold text-brand-ink hover:bg-brand-soft focus-visible:outline-2 focus-visible:outline-brand disabled:opacity-50"
                >
                  Reopen
                </button>
              </li>
            ))}
          </ul>
        )}
        {count > 1 && (
          <button
            type="button"
            disabled={closures.pending}
            onClick={closures.reopenAll}
            className="text-xs font-semibold text-brand-ink underline disabled:opacity-50"
          >
            Reopen everything
          </button>
        )}

        <form
          className="grid grid-cols-1 gap-2 sm:grid-cols-[1fr_1.4fr_auto] sm:items-end"
          onSubmit={(e) => {
            e.preventDefault()
            submit()
          }}
        >
          <label className="text-xs font-semibold text-muted">
            Line
            <select
              value={lineId}
              onChange={(e) => {
                setLineId(e.target.value)
                setTarget('')
              }}
              disabled={lines.length === 0}
              className="mt-1 block h-10 w-full rounded-lg border border-line-strong bg-surface px-2.5 text-sm font-normal text-ink"
            >
              <option value="">Choose a line</option>
              {lines.map((l) => (
                <option key={l.id} value={l.id}>
                  {l.name}
                </option>
              ))}
            </select>
          </label>
          <label className="text-xs font-semibold text-muted">
            Close
            <select
              value={target}
              onChange={(e) => setTarget(e.target.value)}
              disabled={!line}
              className="mt-1 block h-10 w-full rounded-lg border border-line-strong bg-surface px-2.5 text-sm font-normal text-ink disabled:bg-subtle"
            >
              <option value="">{line ? 'Station or section' : 'Pick a line first'}</option>
              {choices.map((c) => (
                <option key={c.value} value={c.value}>
                  {c.label}
                </option>
              ))}
            </select>
          </label>
          <button
            type="submit"
            disabled={!target || closures.pending}
            className="h-10 rounded-full bg-danger px-4 text-sm font-semibold text-white hover:bg-danger-strong focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-danger disabled:opacity-40"
          >
            Close
          </button>
        </form>
      </div>
    </div>
  )
}
