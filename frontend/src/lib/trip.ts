import type { CurvePoint, Endpoint, TourTotals, TripMethod } from '../api/types'

export const METHODS: readonly { id: TripMethod; label: string; color: string }[] = [
  { id: 'hill_climb', label: 'Hill climbing', color: '#2a78d6' },
  { id: 'simulated_annealing', label: 'Simulated annealing', color: '#eb6834' },
]

export function methodInfo(id: TripMethod) {
  return METHODS.find((m) => m.id === id) ?? METHODS[0]!
}

export const MAX_STOPS = 10
export const MIN_STOPS = 3

/** Linear map from a domain to a pixel range. A flat domain maps to the middle. */
export function linearScale(domain: [number, number], range: [number, number]): (v: number) => number {
  const [d0, d1] = domain
  const [r0, r1] = range
  if (d1 === d0) return () => (r0 + r1) / 2
  return (v) => r0 + ((v - d0) / (d1 - d0)) * (r1 - r0)
}

/** Round tick values inside [min, max], about `count` intervals apart (1, 2 or 5 times a power of ten). */
export function niceTicks(min: number, max: number, count = 4): number[] {
  if (!Number.isFinite(min) || !Number.isFinite(max)) return []
  if (max === min) return [min]
  const raw = (max - min) / Math.max(1, count)
  const pow = 10 ** Math.floor(Math.log10(raw))
  // The 1/2/5 step whose interval count is closest to the one asked for.
  const intervals = (s: number) => Math.abs((max - min) / s - count)
  const step = [1, 2, 5, 10].map((m) => m * pow).reduce((a, b) => (intervals(b) < intervals(a) ? b : a))
  const ticks: number[] = []
  for (let t = Math.ceil(min / step) * step; t <= max + step * 1e-9; t += step) ticks.push(Number(t.toPrecision(12)))
  return ticks
}

/** SVG path through the points; `step` holds each value until the next point (for best-so-far). */
export function linePath(
  points: readonly CurvePoint[],
  key: 'best' | 'current',
  x: (v: number) => number,
  y: (v: number) => number,
  step = false,
): string {
  let d = ''
  points.forEach((p, i) => {
    const px = x(p.evaluations).toFixed(1)
    const py = y(p[key]).toFixed(1)
    if (i === 0) d = `M${px},${py}`
    else if (step) d += `H${px}V${py}`
    else d += `L${px},${py}`
  })
  return d
}

/** The curve point at or just before `evaluations` (for the hover read-out). */
export function pointAt(points: readonly CurvePoint[], evaluations: number): CurvePoint | null {
  let found: CurvePoint | null = null
  for (const p of points) {
    if (p.evaluations > evaluations) break
    found = p
  }
  return found
}

/** What the optimised order saves against the order the stops were entered in. */
export function savings(input: TourTotals, best: TourTotals): { min: number; rs: number; pct: number } {
  const min = Math.round(input.total_min - best.total_min)
  const rs = Math.round(input.fare_rs - best.fare_rs)
  const pct = input.score > 0 ? Math.round(((input.score - best.score) / input.score) * 100) : 0
  return { min, rs, pct }
}

/** Stop names in visiting order (a round trip ends back at the start). */
export function orderNames(order: readonly number[], stops: readonly Endpoint[], roundTrip: boolean): string[] {
  const names = order.map((i) => stops[i]?.name ?? `Stop ${i + 1}`)
  if (roundTrip && order.length > 0) names.push(stops[order[0]!]?.name ?? 'Start')
  return names
}

/** Move the item at `from` to `to`, returning a new array. */
export function moveItem<T>(items: readonly T[], from: number, to: number): T[] {
  if (from === to || from < 0 || to < 0 || from >= items.length || to >= items.length) return [...items]
  const next = [...items]
  const [item] = next.splice(from, 1)
  next.splice(to, 0, item!)
  return next
}
