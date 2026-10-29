import { MODE_COLORS } from '../lib/route'

const ITEMS = [
  { label: 'Metro', style: { background: 'linear-gradient(90deg,#4169E1 0 33%,#FFDF00 33% 66%,#CC338B 66%)' } },
  { label: 'Cab', style: { background: `repeating-linear-gradient(90deg,var(--mode-cab) 0 6px,transparent 6px 10px)` } },
  { label: 'Auto', style: { background: `repeating-linear-gradient(90deg,${MODE_COLORS.auto} 0 6px,transparent 6px 10px)` } },
  { label: 'Bike', style: { background: `repeating-linear-gradient(90deg,${MODE_COLORS.bike} 0 6px,transparent 6px 10px)` } },
  { label: 'Walk', style: { background: `repeating-linear-gradient(90deg,${MODE_COLORS.walk} 0 2px,transparent 2px 6px)` } },
]

/** Line styles on the map: under the attribution on phones (the sheet covers the bottom), bottom-left on desktop. */
export function MapLegend() {
  return (
    <div className="pointer-events-none absolute top-8 left-3 z-[1000] max-w-[calc(100%-1.5rem)] rounded-xl border border-line bg-surface/90 px-2.5 py-1.5 shadow-card backdrop-blur lg:top-auto lg:bottom-6">
      <p className="sr-only">Map legend</p>
      <ul className="flex flex-wrap gap-x-3 gap-y-1">
        {ITEMS.map((item) => (
          <li key={item.label} className="flex items-center gap-1.5 text-[11px] font-medium text-ink-2">
            <span className="inline-block h-1 w-5 rounded-full" style={item.style} aria-hidden="true" />
            {item.label}
          </li>
        ))}
      </ul>
    </div>
  )
}
