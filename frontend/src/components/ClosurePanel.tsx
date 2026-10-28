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
    <div className="rounded-lg border border-slate-200 bg-white">
      <button
        type="button"
        aria-expanded={open}
        aria-controls={`${id}-body`}
        onClick={() => setOpen((o) => !o)}
        className="flex w-full items-center gap-2 rounded-lg px-3 py-2 text-left text-sm font-semibold text-slate-800 hover:bg-slate-50 focus-visible:outline-2 focus-visible:outline-teal-700"
      >
        <Construction className="size-4 text-slate-500" aria-hidden="true" />
        Line closures
        {count > 0 ? (
          <span className="rounded-full bg-red-100 px-2 py-0.5 text-xs font-bold text-red-800">{count} active</span>
        ) : (
          <span className="text-xs font-normal text-slate-500">none</span>
        )}
        <ChevronDown className={`ml-auto size-4 text-slate-500 transition-transform ${open ? 'rotate-180' : ''}`} aria-hidden="true" />
      </button>

      {closures.error && (
        <div role="alert" className="mx-3 mb-2 flex items-start gap-2 rounded-md bg-red-50 px-2.5 py-2 text-xs text-red-900">
          <CircleAlert className="mt-px size-4 shrink-0" aria-hidden="true" />
          <span className="flex-1">{closures.error}</span>
          {closures.status === 'error' ? (
            <button type="button" onClick={closures.reload} className="font-semibold underline">
              Retry
            </button>
          ) : (
            <button type="button" onClick={closures.dismissError} aria-label="Dismiss" className="rounded p-0.5 hover:bg-red-100">
              <X className="size-3.5" aria-hidden="true" />
            </button>
          )}
        </div>
      )}

      <div id={`${id}-body`} hidden={!open} className="space-y-3 border-t border-slate-100 px-3 pt-2 pb-3">
        <p className="text-xs text-slate-600">
          Click a station or a line on the map to close it. A closed station is skipped (trains run through without
          stopping); a closed section cuts the line. Every route re-plans around them.
        </p>

        {count > 0 && (
          <ul className="space-y-1.5" aria-label="Active closures">
            {closures.items.map((c) => (
              <li key={c.id} className="flex items-center gap-2 text-xs text-slate-800">
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
                  className="rounded-md px-2 py-1 font-semibold text-teal-800 hover:bg-teal-50 focus-visible:outline-2 focus-visible:outline-teal-700 disabled:opacity-50"
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
            className="text-xs font-semibold text-teal-800 underline disabled:opacity-50"
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
          <label className="text-xs font-semibold text-slate-600">
            Line
            <select
              value={lineId}
              onChange={(e) => {
                setLineId(e.target.value)
                setTarget('')
              }}
              disabled={lines.length === 0}
              className="mt-1 block w-full rounded-md border border-slate-300 bg-white px-2 py-1.5 text-sm font-normal text-slate-900"
            >
              <option value="">Choose a line</option>
              {lines.map((l) => (
                <option key={l.id} value={l.id}>
                  {l.name}
                </option>
              ))}
            </select>
          </label>
          <label className="text-xs font-semibold text-slate-600">
            Close
            <select
              value={target}
              onChange={(e) => setTarget(e.target.value)}
              disabled={!line}
              className="mt-1 block w-full rounded-md border border-slate-300 bg-white px-2 py-1.5 text-sm font-normal text-slate-900 disabled:bg-slate-50"
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
            className="rounded-md bg-red-700 px-3 py-1.5 text-sm font-semibold text-white hover:bg-red-800 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-red-700 disabled:opacity-40"
          >
            Close
          </button>
        </form>
      </div>
    </div>
  )
}
