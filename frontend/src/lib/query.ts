import { endpointRef } from '../api/client'
import type { JourneyRequest, Mode, SearchResult } from '../api/types'

export const ALL_MODES: readonly Mode[] = ['walk', 'metro', 'cab', 'auto', 'bike']

export interface Query {
  from: SearchResult | null
  to: SearchResult | null
  departAt: string
  modes: Mode[]
  preference: number
}

/** 0 = cheapest … 1 = fastest, in words. */
export function preferenceLabel(p: number): string {
  if (p <= 0.1) return 'Cheapest'
  if (p < 0.4) return 'Cheaper'
  if (p <= 0.6) return 'Balanced'
  if (p < 0.9) return 'Faster'
  return 'Fastest'
}

/** The POST /journeys body for a complete query, or null while from/to/modes are missing. */
export function toJourneyRequest(q: Query): JourneyRequest | null {
  if (!q.from || !q.to || q.modes.length === 0) return null
  return {
    from: endpointRef(q.from),
    to: endpointRef(q.to),
    depart_at: q.departAt,
    modes: [...q.modes],
    preference: Math.round(q.preference * 100) / 100,
    max_results: 6,
  }
}
