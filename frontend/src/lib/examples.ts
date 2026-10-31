import type { SearchResult } from '../api/types'

export interface PlaceRef {
  name: string
  id: string
}

export interface ExampleTrip {
  from: PlaceRef
  to: PlaceRef
  note: string
  /** Fair cab price for the trip on a weekday afternoon, from the fitted Uber Go rate card (rounded to ₹10). */
  cab: { name: string; fair: readonly [number, number] }
}

/** Trips offered on the home page, one tap from a plan. */
export const EXAMPLE_TRIPS: readonly ExampleTrip[] = [
  {
    from: { name: 'Bennett University', id: 'bennett-university' },
    to: { name: 'Hauz Khas Village', id: 'hauz-khas-village' },
    note: 'Campus to a night out',
    cab: { name: 'Uber Go', fair: [770, 950] },
  },
  {
    from: { name: 'India Gate', id: 'india-gate' },
    to: { name: 'IGI Airport Terminal 3', id: 'igi-airport-terminal-3' },
    note: 'Catch a flight',
    cab: { name: 'Uber Go', fair: [350, 430] },
  },
  {
    from: { name: 'Cyber Hub', id: 'cyber-hub' },
    to: { name: 'Connaught Place', id: 'connaught-place' },
    note: 'Gurugram to the city centre',
    cab: { name: 'Uber Go', fair: [340, 420] },
  },
  {
    from: { name: 'IGI Airport Terminal 3', id: 'igi-airport-terminal-3' },
    to: { name: 'Cyber Hub', id: 'cyber-hub' },
    note: 'Land and head to Gurugram',
    cab: { name: 'Uber Go', fair: [560, 690] },
  },
  {
    from: { name: 'IGI Airport Terminal 3', id: 'igi-airport-terminal-3' },
    to: { name: 'New Delhi Railway Station', id: 'new-delhi-railway-station' },
    note: 'Flight to a train',
    cab: { name: 'Uber Go', fair: [650, 800] },
  },
  {
    from: { name: 'Noida Sector 18 Market', id: 'noida-sector-18-market' },
    to: { name: 'Connaught Place', id: 'connaught-place' },
    note: 'Noida to the city centre',
    cab: { name: 'Uber Go', fair: [310, 390] },
  },
]

type Search = (query: string) => Promise<SearchResult[]>

/** Look a known place up by name and return the exact match (place ids and station ids can share a name). */
export async function findPlace(ref: PlaceRef, search: Search): Promise<SearchResult | null> {
  const items = await search(ref.name)
  return items.find((p) => p.id === ref.id && p.kind === 'place') ?? items.find((p) => p.id === ref.id) ?? null
}
