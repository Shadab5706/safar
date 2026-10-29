import { describe, expect, it, vi } from 'vitest'
import type { SearchResult } from '../api/types'
import { EXAMPLE_TRIPS, findPlace } from './examples'

const place = (id: string, name: string): SearchResult =>
  ({ kind: 'place', id, name, lat: 28.6, lng: 77.2, category: 'tourist', city: 'Delhi' }) as SearchResult
const station = (id: string, name: string): SearchResult =>
  ({ kind: 'station', id, name, lat: 28.6, lng: 77.2, lines: ['yellow'] }) as SearchResult

describe('findPlace', () => {
  it('returns the result with the exact id, preferring a place over a station of the same id', async () => {
    const search = vi.fn(async () => [station('cyber-hub', 'Cyber Hub'), place('cyber-hub', 'Cyber Hub')])
    const found = await findPlace({ name: 'Cyber Hub', id: 'cyber-hub' }, search)
    expect(search).toHaveBeenCalledWith('Cyber Hub')
    expect(found?.kind).toBe('place')
  })

  it('returns null when nothing matches the id', async () => {
    const found = await findPlace({ name: 'India Gate', id: 'india-gate' }, async () => [place('india-gate-2', 'India Gate Circle')])
    expect(found).toBeNull()
  })
})

describe('EXAMPLE_TRIPS', () => {
  it('has distinct start and end for every trip', () => {
    for (const t of EXAMPLE_TRIPS) expect(t.from.id).not.toBe(t.to.id)
    expect(EXAMPLE_TRIPS).toHaveLength(3)
  })
})
