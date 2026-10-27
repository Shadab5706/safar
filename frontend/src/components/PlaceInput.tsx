import { LoaderCircle, MapPin, TrainFront, X } from 'lucide-react'
import { useEffect, useId, useState, type KeyboardEvent } from 'react'
import { isAbortError, searchPlaces } from '../api/client'
import type { MetroLine, SearchResult } from '../api/types'
import { useDebounced } from '../hooks/useDebounced'

interface Props {
  label: string
  placeholder: string
  value: SearchResult | null
  onChange: (value: SearchResult | null) => void
  lineById: Map<string, MetroLine>
}

interface Found {
  query: string
  items: SearchResult[]
  error?: string
}

function subtitle(item: SearchResult, lineById: Map<string, MetroLine>): string {
  if (item.kind === 'station') {
    const names = item.lines.map((id) => lineById.get(id)?.name ?? id)
    return `Metro station · ${[...new Set(names)].join(', ')}`
  }
  return [item.category, item.city].filter(Boolean).join(' · ')
}

/** Autocomplete over places and metro stations (WAI-ARIA combobox with a listbox popup). */
export function PlaceInput({ label, placeholder, value, onChange, lineById }: Props) {
  const id = useId()
  const listId = `${id}-list`
  const [text, setText] = useState(value?.name ?? '')
  const [open, setOpen] = useState(false)
  const [active, setActive] = useState(-1)
  const [found, setFound] = useState<Found>({ query: '', items: [] })

  // Follow value changes from outside (swap, example trip) without an effect.
  const [shown, setShown] = useState(value)
  if (value !== shown) {
    setShown(value)
    setText(value?.name ?? '')
  }

  const query = text.trim()
  const wanted = open && query.length >= 2 && query !== value?.name
  const debounced = useDebounced(wanted ? query : '', 180)

  useEffect(() => {
    if (debounced.length < 2) return
    const ctl = new AbortController()
    searchPlaces(debounced, ctl.signal)
      .then((items) => setFound({ query: debounced, items }))
      .catch((err: unknown) => {
        if (isAbortError(err)) return
        setFound({ query: debounced, items: [], error: err instanceof Error ? err.message : 'Search failed' })
      })
    return () => ctl.abort()
  }, [debounced])

  const items = wanted ? found.items : []
  const loading = wanted && found.query !== query
  const showList = wanted && (items.length > 0 || !loading)
  const activeItem = active >= 0 ? items[active] : undefined

  function choose(item: SearchResult) {
    onChange(item)
    setText(item.name)
    setOpen(false)
    setActive(-1)
  }

  function onKeyDown(e: KeyboardEvent<HTMLInputElement>) {
    if (e.key === 'ArrowDown') {
      e.preventDefault()
      setOpen(true)
      setActive((i) => (items.length ? (i + 1) % items.length : -1))
    } else if (e.key === 'ArrowUp') {
      e.preventDefault()
      setActive((i) => (items.length ? (i <= 0 ? items.length - 1 : i - 1) : -1))
    } else if (e.key === 'Enter') {
      const pick = activeItem ?? (items.length === 1 ? items[0] : undefined)
      if (open && pick) {
        e.preventDefault()
        choose(pick)
      }
    } else if (e.key === 'Escape') {
      if (open) {
        e.preventDefault()
        setOpen(false)
      }
    }
  }

  return (
    <div className="relative">
      <label htmlFor={id} className="mb-1 block text-xs font-semibold tracking-wide text-slate-600 uppercase">
        {label}
      </label>
      <div className="relative">
        <input
          id={id}
          type="text"
          role="combobox"
          aria-autocomplete="list"
          aria-expanded={showList}
          aria-controls={listId}
          aria-activedescendant={activeItem ? `${id}-opt-${active}` : undefined}
          autoComplete="off"
          spellCheck={false}
          placeholder={placeholder}
          value={text}
          onChange={(e) => {
            setText(e.target.value)
            setOpen(true)
            setActive(-1)
            if (value) onChange(null)
          }}
          onFocus={() => setOpen(true)}
          onBlur={() => setOpen(false)}
          onKeyDown={onKeyDown}
          className="w-full rounded-lg border border-slate-300 bg-white py-2.5 pr-9 pl-3 text-[15px] text-slate-900 shadow-sm placeholder:text-slate-400 focus:border-teal-600 focus:ring-2 focus:ring-teal-600/20 focus:outline-none"
        />
        <span className="absolute inset-y-0 right-2 flex items-center">
          {loading ? (
            <LoaderCircle className="size-4 animate-spin text-slate-400" aria-hidden="true" />
          ) : text ? (
            <button
              type="button"
              aria-label={`Clear ${label.toLowerCase()}`}
              onClick={() => {
                setText('')
                onChange(null)
              }}
              className="rounded p-1 text-slate-400 hover:bg-slate-100 hover:text-slate-700"
            >
              <X className="size-4" aria-hidden="true" />
            </button>
          ) : null}
        </span>
      </div>
      <ul
        id={listId}
        role="listbox"
        aria-label={`${label} suggestions`}
        hidden={!showList}
        className="absolute z-[1000] mt-1 max-h-72 w-full overflow-y-auto rounded-lg border border-slate-200 bg-white py-1 shadow-lg"
      >
        {items.map((item, i) => (
          <li
            key={`${item.kind}:${item.id}`}
            id={`${id}-opt-${i}`}
            role="option"
            aria-selected={i === active}
            onMouseDown={(e) => e.preventDefault()}
            onClick={() => choose(item)}
            onMouseEnter={() => setActive(i)}
            className={`flex cursor-pointer items-start gap-2.5 px-3 py-2 ${i === active ? 'bg-teal-50' : ''}`}
          >
            {item.kind === 'station' ? (
              <TrainFront className="mt-0.5 size-4 shrink-0 text-slate-500" aria-hidden="true" />
            ) : (
              <MapPin className="mt-0.5 size-4 shrink-0 text-slate-500" aria-hidden="true" />
            )}
            <span className="min-w-0">
              <span className="block truncate text-sm font-medium text-slate-900">{item.name}</span>
              <span className="flex items-center gap-1.5 truncate text-xs text-slate-500">
                {item.kind === 'station' &&
                  item.lines.map((l) => (
                    <span
                      key={l}
                      className="inline-block size-2 shrink-0 rounded-full"
                      style={{ backgroundColor: lineById.get(l)?.color ?? '#94a3b8' }}
                      aria-hidden="true"
                    />
                  ))}
                <span className="truncate capitalize">{subtitle(item, lineById)}</span>
              </span>
            </span>
          </li>
        ))}
        {!loading && items.length === 0 && (
          <li role="presentation" className="px-3 py-2 text-sm text-slate-500">
            {found.error ?? 'No places or stations match.'}
          </li>
        )}
      </ul>
    </div>
  )
}
