import { useEffect, useRef, useState, type PointerEvent, type ReactNode } from 'react'
import { MapInset } from '../hooks/useMapInset'
import { DESKTOP, useMediaQuery } from '../hooks/useMediaQuery'
import { clampOffset, nextSnap, settleSnap, snapOffsets, type Snap } from '../lib/sheet'

interface Gesture {
  y0: number
  start: number
  lastY: number
  lastT: number
  v: number
  moved: boolean
}

interface Props {
  /** Accessible name of the panel ("Journey planner", "Algorithm Lab"…). */
  label: string
  panel: ReactNode
  map: ReactNode
  snap: Snap
  onSnap: (snap: Snap) => void
}

/**
 * A screen with a map and a panel of controls and results. Large screens put the panel beside
 * the map; phones put it in a bottom sheet over the map that can be dragged or tapped between
 * peek, half and full height. The panel and map stay mounted across the switch.
 */
export function MapScreen({ label, panel, map, snap, onSnap }: Props) {
  const desktop = useMediaQuery(DESKTOP)
  const box = useRef<HTMLDivElement>(null)
  const [height, setHeight] = useState(() => (typeof window === 'undefined' ? 0 : window.innerHeight - 120))
  const [drag, setDrag] = useState<number | null>(null)
  const gesture = useRef<Gesture | null>(null)
  const dragged = useRef(false)

  useEffect(() => {
    const el = box.current
    if (!el) return
    // Hidden screens measure 0: keep the last real height so the sheet doesn't jump when shown again.
    const ro = new ResizeObserver(() => {
      if (el.clientHeight > 0) setHeight(el.clientHeight)
    })
    ro.observe(el)
    return () => ro.disconnect()
  }, [])

  const sheet = !desktop
  const offsets = snapOffsets(height)
  const y = drag ?? offsets[snap]
  const inset = sheet ? Math.max(0, height - offsets[snap === 'full' ? 'half' : snap]) : 0

  function down(e: PointerEvent<HTMLElement>) {
    e.currentTarget.setPointerCapture(e.pointerId)
    gesture.current = { y0: e.clientY, start: offsets[snap], lastY: e.clientY, lastT: e.timeStamp, v: 0, moved: false }
  }

  function move(e: PointerEvent<HTMLElement>) {
    const g = gesture.current
    if (!g) return
    const dy = e.clientY - g.y0
    if (!g.moved && Math.abs(dy) < 6) return
    g.moved = true
    const dt = e.timeStamp - g.lastT
    if (dt > 0) g.v = (e.clientY - g.lastY) / dt
    g.lastY = e.clientY
    g.lastT = e.timeStamp
    setDrag(clampOffset(g.start + dy, offsets))
  }

  function up(e: PointerEvent<HTMLElement>) {
    const g = gesture.current
    gesture.current = null
    if (!g?.moved) return
    dragged.current = true
    setDrag(null)
    onSnap(settleSnap(clampOffset(g.start + e.clientY - g.y0, offsets), g.v, offsets))
  }

  return (
    <div ref={box} className="relative h-full overflow-clip lg:grid lg:grid-cols-[minmax(360px,440px)_1fr]">
      <section aria-label="Map" className="absolute inset-0 lg:relative lg:col-start-2 lg:row-start-1 lg:h-full lg:min-h-0">
        <MapInset.Provider value={inset}>{map}</MapInset.Provider>
      </section>
      <aside
        aria-label={label}
        className={
          sheet
            ? 'absolute inset-x-0 top-0 z-[1100] flex flex-col rounded-t-[1.75rem] border-t border-line bg-surface shadow-pop'
            : 'flex min-h-0 flex-col border-r border-line bg-surface lg:col-start-1 lg:row-start-1'
        }
        style={
          sheet
            ? {
                height: height - offsets.full,
                transform: `translate3d(0, ${y}px, 0)`,
                transition: drag === null ? 'transform 380ms var(--ease-soft)' : 'none',
              }
            : undefined
        }
      >
        {sheet && (
          <button
            type="button"
            aria-label={snap === 'full' ? `Shrink ${label.toLowerCase()} to show the map` : `Expand ${label.toLowerCase()}`}
            aria-expanded={snap === 'full'}
            onPointerDown={down}
            onPointerMove={move}
            onPointerUp={up}
            onPointerCancel={() => {
              gesture.current = null
              setDrag(null)
            }}
            onClick={() => {
              if (dragged.current) dragged.current = false
              else onSnap(nextSnap(snap))
            }}
            className="flex h-11 w-full shrink-0 cursor-grab touch-none items-center justify-center rounded-t-[1.75rem] focus-visible:outline-2 focus-visible:-outline-offset-4 focus-visible:outline-brand active:cursor-grabbing"
          >
            <span className="h-1.5 w-10 rounded-full bg-line-strong" aria-hidden="true" />
          </button>
        )}
        <div
          className="min-h-0 flex-1 overflow-y-auto overscroll-contain"
          style={sheet ? { paddingBottom: Math.max(0, y - offsets.full) } : undefined}
        >
          {panel}
        </div>
      </aside>
    </div>
  )
}
