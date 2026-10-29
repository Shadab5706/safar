import { LoaderCircle, MapPin, TrainFront, X } from 'lucide-react'
import { useEffect, useId, useState, type KeyboardEvent, type ReactNode } from 'react'
import { isAbortError, searchPlaces } from '../api/client'
import type { MetroLine, SearchResult } from '../api/types'
import { useDebounced } from '../hooks/useDebounced'

interface Props {
  label: string
  placeholder: string
  value: SearchResult | null
  onChange: (value: SearchResult | null) => void
  lineById: Map<string, MetroLine>
  /** Only offer metro stations (the Algorithm Lab searches the metro graph). */
  stationsOnly?: boolean
  /** Icon shown inside the field, on the left. */
  leading?: ReactNode
  /** Keep the label for screen readers only (the field sits in a labelled group). */
  hideLabel?: boolean
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
export function PlaceInput({ label, placeholder, value, onChange, lineById, stationsOnly = false, leading, hideLabel = false }: Props) {
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
    searchPlaces(debounced, ctl.signal, stationsOnly ? 25 : 8)
      .then((items) => setFound({ query: debounced, items: stationsOnly ? items.filter((i) => i.kind === 'station').slice(0, 8) : items }))
      .catch((err: unknown) => {
        if (isAbortError(err)) return
        setFound({ query: debounced, items: [], error: err instanceof Error ? err.message : 'Search failed' })
      })
    return () => ctl.abort()
  }, [debounced, stationsOnly])

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
      <label htmlFor={id} className={hideLabel ? 'sr-only' : 'mb-1.5 block text-xs font-semibold tracking-wide text-muted uppercase'}>
        {label}
      </label>
      <div className="relative">
        {leading && <span className="pointer-events-none absolute inset-y-0 left-3.5 flex items-center">{leading}</span>}
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
          className={`h-12 w-full rounded-control border border-line-strong bg-surface pr-11 text-base text-ink shadow-card transition-colors placeholder:text-faint hover:border-faint focus:border-brand focus:ring-4 focus:ring-brand/15 focus:outline-none ${leading ? 'pl-10' : 'pl-3.5'}`}
        />
        <span className="absolute inset-y-0 right-1 flex items-center">
          {loading ? (
            <span className="grid size-10 place-items-center">
              <LoaderCircle className="size-4 animate-spin text-faint" aria-hidden="true" />
            </span>
          ) : text ? (
            <button
              type="button"
              aria-label={`Clear ${label.toLowerCase()}`}
              onClick={() => {
                setText('')
                onChange(null)
              }}
              className="grid size-10 place-items-center rounded-full text-faint hover:bg-subtle hover:text-ink-2 focus-visible:outline-2 focus-visible:outline-brand"
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
        className="absolute z-[1200] mt-1.5 max-h-72 w-full animate-fade overflow-y-auto rounded-control border border-line bg-surface p-1 shadow-pop"
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
            className={`flex min-h-11 cursor-pointer items-start gap-2.5 rounded-lg px-3 py-2 ${i === active ? 'bg-brand-soft' : ''}`}
          >
            {item.kind === 'station' ? (
              <TrainFront className="mt-0.5 size-4 shrink-0 text-muted" aria-hidden="true" />
            ) : (
              <MapPin className="mt-0.5 size-4 shrink-0 text-muted" aria-hidden="true" />
            )}
            <span className="min-w-0">
              <span className="block truncate text-sm font-medium text-ink">{item.name}</span>
              <span className="flex items-center gap-1.5 truncate text-xs text-muted">
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
          <li role="presentation" className="px-3 py-2 text-sm text-muted">
            {found.error ?? 'No places or stations match.'}
          </li>
        )}
      </ul>
    </div>
  )
}
