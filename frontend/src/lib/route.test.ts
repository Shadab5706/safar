import { describe, expect, it } from 'vitest'
import type { Endpoint, FareQuote, Leg, Route } from '../api/types'
import { ALL_MODES, preferenceLabel, toJourneyRequest } from './query'
import {
  drawableLegs,
  fareBreakdown,
  hasEstimatedFares,
  legStartOffsets,
  legStyle,
  routeBounds,
  routeChain,
  sortQuotes,
  textOn,
} from './route'

const at = (id: string, lat: number, lng: number): Endpoint => ({ id, name: id, type: 'station', lat, lng })
const A = at('a', 28.45, 77.58)
const B = at('b', 28.46, 77.5)
const C = at('c', 28.58, 77.37)

const quote = (provider: string, mode: FareQuote['mode'], fare: number): FareQuote => ({
  provider,
  provider_name: provider,
  product: mode,
  product_name: `${provider} ${mode}`,
  mode,
  fare_rs: fare,
  fare_range_rs: [fare - 10, fare + 10],
  surge: 1,
  pickup_eta_min: 4,
  ride_min: 20,
  total_min: 24,
  distance_km: 10,
  source: 'estimate',
  basis: 'approximate',
  deep_link: null,
})

const legs: Leg[] = [
  {
    kind: 'road', mode: 'bike', from: A, to: B, distance_km: 11.2, total_min: 28.1, ride_min: 24.1, wait_min: 4,
    fare_rs: 114, fare_source: 'estimate', road_data: 'osrm', points: [[A.lat, A.lng], [B.lat, B.lng]],
    options: [quote('uber', 'cab', 284), quote('rapido', 'bike', 114), quote('meter', 'auto', 148)],
  },
  {
    kind: 'metro', line: { id: 'aqua', name: 'Aqua Line', color: '#00B5E2', mode: 'metro' }, from: B, to: C,
    distance_km: 25.9, total_min: 51.5, ride_min: 41, wait_min: 4, entry_min: 4, exit_min: 2.5, stops: 16, ticket: 0,
    points: [[B.lat, B.lng], [28.5, 77.45], [C.lat, C.lng]],
  },
  { kind: 'transfer', from: C, to: C, distance_km: 0, total_min: 4, walk_min: 4, points: [[C.lat, C.lng]] },
  { kind: 'walk', from: C, to: A, distance_km: 0.9, total_min: 12.2, walk_min: 12.2, points: [[C.lat, C.lng], [28.6, 77.2]] },
]

const route: Route = {
  rank: 1, summary: 'Bike taxi → Aqua Line → Walk', tags: ['recommended', 'cheapest'], total_min: 95.8, fare_rs: 169,
  distance_km: 38, transfers: 1, walk_min: 16.2, score: 1, found_by: 'tradeoff', dominated: false, legs,
  tickets: [{ zone: 'NMRC', from: B, to: C, distance_km: 25.9, fare_rs: 55, fare_verified: false }],
}

describe('route helpers', () => {
  it('styles legs by mode and metro line', () => {
    expect(legStyle(legs[1]!)).toEqual({ color: '#00B5E2', weight: 6 })
    expect(legStyle(legs[0]!).dashArray).toBeDefined()
    expect(legStyle(legs[3]!).color).toBe('#64748b')
  })

  it('skips single-point transfers when drawing', () => {
    expect(drawableLegs(route).map((l) => l.kind)).toEqual(['road', 'metro', 'walk'])
  })

  it('computes bounds over every leg point', () => {
    expect(routeBounds(route)).toEqual([
      [28.45, 77.2],
      [28.6, 77.58],
    ])
    expect(routeBounds({ ...route, legs: [] })).toBeNull()
  })

  it('splits the fare into metro tickets and road estimates', () => {
    expect(fareBreakdown(route)).toEqual({ metro: 55, road: 114 })
    expect(hasEstimatedFares(route)).toBe(true)
    expect(hasEstimatedFares({ ...route, legs: [legs[1]!] })).toBe(false)
  })

  it('lists the leg vehicle quotes first, cheapest first', () => {
    const sorted = sortQuotes(legs[0]!.kind === 'road' ? legs[0]!.options : [], 'cab')
    expect(sorted.map((q) => q.provider)).toEqual(['uber', 'rapido', 'meter'])
  })

  it('builds the chip chain without changes', () => {
    expect(routeChain(route).map((s) => s.label)).toEqual(['Bike taxi', 'Aqua Line', 'Walk'])
  })

  it('offsets each leg start by the time before it', () => {
    expect(legStartOffsets(route)).toEqual([0, 28.1, 79.6, 83.6].map((n) => expect.closeTo(n, 5)))
  })

  it('picks readable text on line colours', () => {
    expect(textOn('#FFDF00')).toBe('#000000')
    expect(textOn('#4169E1')).toBe('#ffffff')
    expect(textOn('#553592')).toBe('#ffffff')
    expect(textOn('bad')).toBe('#ffffff')
  })
})

describe('journey request', () => {
  const from = { kind: 'place' as const, id: 'bennett-university', name: 'Bennett University', lat: 0, lng: 0 }
  const to = { kind: 'station' as const, id: 'hauz-khas', name: 'Hauz Khas', lat: 0, lng: 0, lines: ['yellow'] }

  it('builds the POST body with typed endpoint refs', () => {
    expect(
      toJourneyRequest({ from, to, departAt: '2026-10-27T09:00', modes: [...ALL_MODES], preference: 0.333 }),
    ).toEqual({
      from: 'place:bennett-university',
      to: 'station:hauz-khas',
      depart_at: '2026-10-27T09:00',
      modes: ['walk', 'metro', 'cab', 'auto', 'bike'],
      preference: 0.33,
      max_results: 6,
    })
  })

  it('is null until the query is complete', () => {
    expect(toJourneyRequest({ from, to: null, departAt: '', modes: ['metro'], preference: 0.5 })).toBeNull()
    expect(toJourneyRequest({ from, to, departAt: '', modes: [], preference: 0.5 })).toBeNull()
  })

  it('names the preference', () => {
    expect(preferenceLabel(0)).toBe('Cheapest')
    expect(preferenceLabel(0.5)).toBe('Balanced')
    expect(preferenceLabel(1)).toBe('Fastest')
  })
})
