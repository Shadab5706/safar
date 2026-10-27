import type { Mode, RouteTag } from '../api/types'

const rupees = new Intl.NumberFormat('en-IN', { maximumFractionDigits: 0 })

/** 154.7 -> "2 h 35 min", 45 -> "45 min", 0.3 -> "1 min". */
export function formatDuration(minutes: number): string {
  if (!Number.isFinite(minutes) || minutes <= 0) return '0 min'
  const total = Math.max(1, Math.round(minutes))
  const h = Math.floor(total / 60)
  const m = total % 60
  if (h === 0) return `${m} min`
  return m === 0 ? `${h} h` : `${h} h ${m} min`
}

/** 1234.4 -> "₹1,234" (Indian digit grouping). */
export function formatRupees(amount: number): string {
  return `₹${rupees.format(Math.round(amount))}`
}

/** [97, 131] -> "₹97–131". */
export function formatRupeeRange([low, high]: readonly [number, number]): string {
  if (Math.round(low) === Math.round(high)) return formatRupees(low)
  return `${formatRupees(low)}–${rupees.format(Math.round(high))}`
}

/** 62.94 -> "62.9 km", 0.34 -> "340 m", 123.4 -> "123 km". */
export function formatDistance(km: number): string {
  if (!Number.isFinite(km) || km <= 0) return '0 m'
  if (km < 1) return `${Math.round((km * 1000) / 10) * 10} m`
  if (km >= 100) return `${Math.round(km)} km`
  return `${km.toFixed(1)} km`
}

export function formatChanges(transfers: number): string {
  if (transfers <= 0) return 'No changes'
  return transfers === 1 ? '1 change' : `${transfers} changes`
}

const TAG_LABELS: Record<RouteTag, string> = {
  recommended: 'Recommended',
  fastest: 'Fastest',
  cheapest: 'Cheapest',
  fewest_changes: 'Fewest changes',
  most_expensive: 'Most expensive',
}

export function tagLabel(tag: string): string {
  return TAG_LABELS[tag as RouteTag] ?? tag.replace(/_/g, ' ')
}

const MODE_LABELS: Record<Mode, string> = {
  walk: 'Walk',
  metro: 'Metro',
  cab: 'Cab',
  auto: 'Auto',
  bike: 'Bike taxi',
}

export function modeLabel(mode: Mode): string {
  return MODE_LABELS[mode]
}

const ZONE_LABELS: Record<string, string> = {
  DMRC: 'Delhi Metro (DMRC)',
  'DMRC-AE': 'Airport Express (DMRC)',
  NMRC: 'Noida Metro (NMRC)',
  RAPID: 'Rapid Metro Gurugram',
  NCRTC: 'Namo Bharat (NCRTC)',
}

export function zoneLabel(zone: string): string {
  return ZONE_LABELS[zone] ?? zone
}

const pad = (n: number) => String(n).padStart(2, '0')

/** A Date as the value of an <input type="datetime-local">: "2026-10-27T09:05". */
export function toLocalInputValue(date: Date): string {
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}T${pad(date.getHours())}:${pad(date.getMinutes())}`
}

/** The next quarter hour from `now`, for the default departure time. */
export function nextQuarterHour(now: Date): Date {
  const d = new Date(now)
  d.setSeconds(0, 0)
  d.setMinutes(Math.ceil(d.getMinutes() / 15) * 15)
  return d
}

const LOCAL_RE = /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2})/

/** Parse "2026-10-27T09:00" as local wall-clock time (never UTC). */
export function parseLocal(value: string): Date | null {
  const m = LOCAL_RE.exec(value)
  if (!m) return null
  const [, y, mo, d, h, mi] = m.map(Number) as [number, number, number, number, number, number]
  return new Date(y, mo - 1, d, h, mi)
}

/** "09:00" for a local datetime string, with `plusMinutes` added. */
export function formatClock(value: string, plusMinutes = 0): string {
  const d = parseLocal(value)
  if (!d) return ''
  d.setMinutes(d.getMinutes() + Math.round(plusMinutes))
  return `${pad(d.getHours())}:${pad(d.getMinutes())}`
}
