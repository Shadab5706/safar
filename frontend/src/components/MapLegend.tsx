import { MODE_COLORS } from '../lib/route'

const ITEMS = [
  { label: 'Metro', style: { background: 'linear-gradient(90deg,#4169E1 0 33%,#FFDF00 33% 66%,#CC338B 66%)' } },
  { label: 'Cab', style: { background: `repeating-linear-gradient(90deg,${MODE_COLORS.cab} 0 6px,transparent 6px 10px)` } },
  { label: 'Auto', style: { background: `repeating-linear-gradient(90deg,${MODE_COLORS.auto} 0 6px,transparent 6px 10px)` } },
  { label: 'Bike', style: { background: `repeating-linear-gradient(90deg,${MODE_COLORS.bike} 0 6px,transparent 6px 10px)` } },
  { label: 'Walk', style: { background: `repeating-linear-gradient(90deg,${MODE_COLORS.walk} 0 2px,transparent 2px 6px)` } },
]

export function MapLegend() {
  return (
    <div className="pointer-events-none absolute bottom-6 left-2 z-[1000] rounded-lg bg-white/90 px-2.5 py-1.5 shadow">
      <p className="sr-only">Map legend</p>
      <ul className="flex flex-wrap gap-x-3 gap-y-1">
        {ITEMS.map((item) => (
          <li key={item.label} className="flex items-center gap-1.5 text-[11px] font-medium text-slate-700">
            <span className="inline-block h-1 w-5 rounded-full" style={item.style} aria-hidden="true" />
            {item.label}
          </li>
        ))}
      </ul>
    </div>
  )
}
