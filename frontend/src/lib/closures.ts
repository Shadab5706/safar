import type { Closure, MetroLine, MetroStation } from '../api/types'

export const CLOSED_COLOR = '#b91c1c'

/** Same ids the backend uses: "station:<id>" and "segment:<line>:<a>:<b>" with a < b. */
export function stationClosureId(station: string): string {
  return `station:${station}`
}

export function segmentClosureId(line: string, a: string, b: string): string {
  const [x, y] = a < b ? [a, b] : [b, a]
  return `segment:${line}:${x}:${y}`
}

export interface ClosedSets {
  stations: Set<string>
  segments: Set<string>
}

export function closedSets(items: Closure[]): ClosedSets {
  const stations = new Set<string>()
  const segments = new Set<string>()
  for (const c of items) {
    if (c.type === 'station') stations.add(c.station.id)
    else segments.add(c.id)
  }
  return { stations, segments }
}

/** One section of track between two neighbouring stations of a line. */
export interface NetworkSegment {
  id: string
  line: MetroLine
  a: MetroStation
  b: MetroStation
}

/** Every section of every line, in running order (loop lines list a station twice; repeats are dropped). */
export function networkSegments(lines: MetroLine[], stationById: Map<string, MetroStation>): NetworkSegment[] {
  const out: NetworkSegment[] = []
  const seen = new Set<string>()
  for (const line of lines) {
    for (let i = 0; i + 1 < line.stations.length; i++) {
      const a = stationById.get(line.stations[i] ?? '')
      const b = stationById.get(line.stations[i + 1] ?? '')
      if (!a || !b || a.id === b.id) continue
      const id = segmentClosureId(line.id, a.id, b.id)
      if (seen.has(id)) continue
      seen.add(id)
      out.push({ id, line, a, b })
    }
  }
  return out
}
