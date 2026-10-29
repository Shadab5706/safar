import type { SearchResult } from '../api/types'

export interface PlaceRef {
  name: string
  id: string
}

export interface ExampleTrip {
  from: PlaceRef
  to: PlaceRef
  note: string
}

/** Trips offered on the home page, one tap from a plan. */
export const EXAMPLE_TRIPS: readonly ExampleTrip[] = [
  {
    from: { name: 'Bennett University', id: 'bennett-university' },
    to: { name: 'Hauz Khas Village', id: 'hauz-khas-village' },
    note: 'Campus to a night out',
  },
  {
    from: { name: 'India Gate', id: 'india-gate' },
    to: { name: 'IGI Airport Terminal 3', id: 'igi-airport-terminal-3' },
    note: 'Catch a flight',
  },
  {
    from: { name: 'Cyber Hub', id: 'cyber-hub' },
    to: { name: 'Connaught Place', id: 'connaught-place' },
    note: 'Gurugram to the city centre',
  },
]

type Search = (query: string) => Promise<SearchResult[]>

/** Look a known place up by name and return the exact match (place ids and station ids can share a name). */
export async function findPlace(ref: PlaceRef, search: Search): Promise<SearchResult | null> {
  const items = await search(ref.name)
  return items.find((p) => p.id === ref.id && p.kind === 'place') ?? items.find((p) => p.id === ref.id) ?? null
}
