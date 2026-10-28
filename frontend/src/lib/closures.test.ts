import { describe, expect, it } from 'vitest'
import type { Closure, MetroLine, MetroStation } from '../api/types'
import { closedSets, networkSegments, segmentClosureId, stationClosureId } from './closures'

const st = (id: string): MetroStation => ({ id, name: id, lat: 28.6, lng: 77.2, lines: [] })
const line = (id: string, stations: string[]): MetroLine => ({
  id,
  name: id,
  color: '#000',
  operator: 'DMRC',
  mode: 'metro',
  avg_wait_min: 3,
  stations,
})

describe('closure ids', () => {
  it('match the backend format, with segment ends sorted', () => {
    expect(stationClosureId('rajiv-chowk')).toBe('station:rajiv-chowk')
    expect(segmentClosureId('yellow', 'hauz-khas', 'green-park')).toBe('segment:yellow:green-park:hauz-khas')
    expect(segmentClosureId('yellow', 'green-park', 'hauz-khas')).toBe('segment:yellow:green-park:hauz-khas')
  })
})

describe('closedSets', () => {
  it('splits stations and segments', () => {
    const ref = { id: 'a', name: 'A', lat: 0, lng: 0 }
    const items: Closure[] = [
      { id: 'station:a', type: 'station', label: '', station: ref },
      { id: 'segment:l:a:b', type: 'segment', label: '', line: { id: 'l', name: 'L', color: '#000' }, from: ref, to: { ...ref, id: 'b' } },
    ]
    const sets = closedSets(items)
    expect([...sets.stations]).toEqual(['a'])
    expect([...sets.segments]).toEqual(['segment:l:a:b'])
  })
})

describe('networkSegments', () => {
  const byId = new Map(['a', 'b', 'c'].map((id) => [id, st(id)]))

  it('pairs neighbouring stations in running order', () => {
    const segs = networkSegments([line('l', ['a', 'b', 'c'])], byId)
    expect(segs.map((s) => s.id)).toEqual(['segment:l:a:b', 'segment:l:b:c'])
    expect(segs[0]?.a.id).toBe('a')
  })

  it('drops repeats from loop lines and unknown stations', () => {
    const segs = networkSegments([line('loop', ['a', 'b', 'c', 'a', 'b', 'x'])], byId)
    expect(segs.map((s) => s.id)).toEqual(['segment:loop:a:b', 'segment:loop:b:c', 'segment:loop:a:c'])
  })
})
