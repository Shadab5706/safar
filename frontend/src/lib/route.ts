import type { FareQuote, LatLng, Leg, RoadMode, Route } from '../api/types'
import { modeLabel } from './format'

export const MODE_COLORS: Record<RoadMode | 'walk', string> = {
  cab: '#1e293b',
  auto: '#15803d',
  bike: '#c2410c',
  walk: '#64748b',
}

export interface LegStyle {
  color: string
  weight: number
  dashArray?: string
}

export function legColor(leg: Leg): string {
  switch (leg.kind) {
    case 'metro':
      return leg.line.color
    case 'road':
      return MODE_COLORS[leg.mode]
    default:
      return MODE_COLORS.walk
  }
}

/**
 * Leg colour for the page itself (not the map): the cab colour is near-black, so on screen it
 * comes from a token that turns light in dark mode.
 */
export function uiLegColor(leg: Leg): string {
  return leg.kind === 'road' && leg.mode === 'cab' ? 'var(--mode-cab)' : legColor(leg)
}

/** Each timed leg's share of the whole journey (0..1), for the little time bar on a route card. */
export function timeShares(route: Route): { index: number; share: number }[] {
  const total = route.legs.reduce((sum, leg) => sum + Math.max(0, leg.total_min), 0)
  if (total <= 0) return []
  return route.legs.flatMap((leg, index) => (leg.total_min > 0 ? [{ index, share: leg.total_min / total }] : []))
}

/** Map style per leg: metro solid in its line colour, road dashed, walking dotted. */
export function legStyle(leg: Leg): LegStyle {
  const color = legColor(leg)
  switch (leg.kind) {
    case 'metro':
      return { color, weight: 6 }
    case 'road':
      return { color, weight: 5, dashArray: '10 7' }
    default:
      return { color, weight: 4, dashArray: '2 8' }
  }
}

export function legTitle(leg: Leg): string {
  switch (leg.kind) {
    case 'metro':
      return leg.line.name
    case 'road':
      return modeLabel(leg.mode)
    case 'walk':
      return 'Walk'
    case 'transfer':
      return `Change at ${leg.from.name}`
  }
}

/** Legs that can be drawn on the map (a transfer is a single point). */
export function drawableLegs(route: Route): Leg[] {
  return route.legs.filter((leg) => leg.points.length >= 2)
}

export type Bounds = [LatLng, LatLng]

/** South-west and north-east corners around every point of the route. */
export function routeBounds(route: Route): Bounds | null {
  let minLat = Infinity
  let minLng = Infinity
  let maxLat = -Infinity
  let maxLng = -Infinity
  for (const leg of route.legs) {
    for (const [lat, lng] of leg.points) {
      minLat = Math.min(minLat, lat)
      minLng = Math.min(minLng, lng)
      maxLat = Math.max(maxLat, lat)
      maxLng = Math.max(maxLng, lng)
    }
  }
  if (!Number.isFinite(minLat)) return null
  return [
    [minLat, minLng],
    [maxLat, maxLng],
  ]
}

export function hasEstimatedFares(route: Route): boolean {
  return route.legs.some((leg) => leg.kind === 'road')
}

/** Quotes for the leg's own vehicle type first, then every other vehicle; cheapest first in each. */
export function sortQuotes(options: readonly FareQuote[], mode: RoadMode): FareQuote[] {
  return [...options].sort((a, b) => {
    const am = a.mode === mode ? 0 : 1
    const bm = b.mode === mode ? 0 : 1
    return am - bm || a.fare_rs - b.fare_rs
  })
}

export interface ChainStep {
  key: string
  label: string
  color: string
  kind: Leg['kind']
}

/** The route as a short chain of chips: "Bike taxi › Aqua Line › Walk › …" (changes are left out). */
export function routeChain(route: Route): ChainStep[] {
  return route.legs
    .map((leg, i) => ({ leg, i }))
    .filter(({ leg }) => leg.kind !== 'transfer')
    .map(({ leg, i }) => ({ key: `${i}`, label: legTitle(leg), color: legColor(leg), kind: leg.kind }))
}

/** A route made only of one cab/auto/bike ride. */
export function isDirectRide(route: Route): boolean {
  return route.legs.length === 1 && route.legs[0]?.kind === 'road'
}

/** Metro tickets vs. cab/auto/bike estimates that make up the route fare. */
export function fareBreakdown(route: Route): { metro: number; road: number } {
  const metro = route.tickets.reduce((sum, t) => sum + t.fare_rs, 0)
  const road = route.legs.reduce((sum, leg) => sum + (leg.kind === 'road' ? leg.fare_rs : 0), 0)
  return { metro, road }
}

/** Minutes after departure at which each leg starts. */
export function legStartOffsets(route: Route): number[] {
  let t = 0
  return route.legs.map((leg) => {
    const start = t
    t += leg.total_min
    return start
  })
}

/** Black or white, whichever reads better on `hex` (WCAG relative luminance). */
export function textOn(hex: string): '#000000' | '#ffffff' {
  const m = /^#?([0-9a-f]{6})$/i.exec(hex.trim())
  if (!m?.[1]) return '#ffffff'
  const n = parseInt(m[1], 16)
  const channel = (c: number) => {
    const s = c / 255
    return s <= 0.03928 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4
  }
  const lum = 0.2126 * channel((n >> 16) & 255) + 0.7152 * channel((n >> 8) & 255) + 0.0722 * channel(n & 255)
  // Contrast with black is (L + 0.05) / 0.05, with white 1.05 / (L + 0.05); they are equal at L ≈ 0.179.
  return lum > 0.179 ? '#000000' : '#ffffff'
}

/** Bounds around a set of points, or null for none. */
export function boundsOf(points: readonly { lat: number; lng: number }[]): Bounds | null {
  if (points.length === 0) return null
  const lats = points.map((p) => p.lat)
  const lngs = points.map((p) => p.lng)
  return [
    [Math.min(...lats), Math.min(...lngs)],
    [Math.max(...lats), Math.max(...lngs)],
  ]
}
