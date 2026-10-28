import type { Algorithm, LabResult } from '../api/types'

export interface AlgorithmInfo {
  id: Algorithm
  label: string
  short: string
  color: string
  blurb: string
}

// Categorical slots 1-5 of the chart palette, in a fixed order (checked for colour-blind separation).
export const ALGORITHMS: readonly AlgorithmInfo[] = [
  { id: 'bfs', label: 'Breadth-first search', short: 'BFS', color: '#2a78d6', blurb: 'Fewest steps, ignores time' },
  { id: 'dfs', label: 'Depth-first search', short: 'DFS', color: '#eb6834', blurb: 'Dives deep, first path wins' },
  { id: 'ucs', label: 'Uniform-cost search', short: 'UCS', color: '#1baf7a', blurb: 'Cheapest first, always optimal' },
  { id: 'greedy', label: 'Greedy best-first', short: 'Greedy', color: '#eda100', blurb: 'Heads straight for the goal' },
  { id: 'astar', label: 'A* search', short: 'A*', color: '#e87ba4', blurb: 'Cost so far + distance left' },
]

export const ALGORITHM_BY_ID = new Map(ALGORITHMS.map((a) => [a.id, a]))

export function algorithmInfo(id: Algorithm): AlgorithmInfo {
  return ALGORITHM_BY_ID.get(id) ?? ALGORITHMS[0]!
}

/** "rajiv-chowk@yellow" (a platform) and "rajiv-chowk" (the street gate) are both Rajiv Chowk. */
export function stationOf(nodeId: string): string {
  const at = nodeId.indexOf('@')
  return at < 0 ? nodeId : nodeId.slice(0, at)
}

/** The first time the search expanded each station: one dot per station on the map. */
export interface Frame {
  station: string
  step: number // index into the raw expansion order
}

export function expansionFrames(expansion: readonly string[]): Frame[] {
  const seen = new Set<string>()
  const frames: Frame[] = []
  expansion.forEach((node, step) => {
    const station = stationOf(node)
    if (seen.has(station)) return
    seen.add(station)
    frames.push({ station, step })
  })
  return frames
}

/** How many frames are visible once the animation has reached `step` (binary search). */
export function framesUpTo(frames: readonly Frame[], step: number): number {
  let lo = 0
  let hi = frames.length
  while (lo < hi) {
    const mid = (lo + hi) >> 1
    if (frames[mid]!.step <= step) lo = mid + 1
    else hi = mid
  }
  return lo
}

/** Bar length as a share of the longest bar, 0-100. A zero maximum gives empty bars. */
export function barPercent(value: number, max: number): number {
  if (!(max > 0) || !(value > 0)) return 0
  return Math.min(100, (value / max) * 100)
}

/** Best (lowest) value among found results, for highlighting winners in the table. */
export function bestOf(results: readonly LabResult[], pick: (r: LabResult) => number): number | null {
  const values = results.filter((r) => r.found).map(pick)
  return values.length ? Math.min(...values) : null
}
