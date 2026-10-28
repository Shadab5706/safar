import { describe, expect, it } from 'vitest'
import type { LabResult } from '../api/types'
import { ALGORITHMS, algorithmInfo, barPercent, bestOf, expansionFrames, framesUpTo, stationOf } from './lab'

describe('stationOf', () => {
  it('maps platforms and gates to their station', () => {
    expect(stationOf('rajiv-chowk@yellow')).toBe('rajiv-chowk')
    expect(stationOf('rajiv-chowk')).toBe('rajiv-chowk')
  })
})

describe('expansionFrames', () => {
  const order = ['rajiv-chowk', 'rajiv-chowk@blue', 'rajiv-chowk@yellow', 'patel-chowk@yellow', 'barakhamba@blue', 'patel-chowk']
  const frames = expansionFrames(order)

  it('keeps the first expansion of each station, with its raw step', () => {
    expect(frames).toEqual([
      { station: 'rajiv-chowk', step: 0 },
      { station: 'patel-chowk', step: 3 },
      { station: 'barakhamba', step: 4 },
    ])
  })

  it('counts the frames visible at a step', () => {
    expect(framesUpTo(frames, -1)).toBe(0)
    expect(framesUpTo(frames, 0)).toBe(1)
    expect(framesUpTo(frames, 2)).toBe(1)
    expect(framesUpTo(frames, 3)).toBe(2)
    expect(framesUpTo(frames, 99)).toBe(3)
    expect(framesUpTo([], 5)).toBe(0)
  })
})

describe('chart helpers', () => {
  it('scales bars against the longest one', () => {
    expect(barPercent(50, 200)).toBe(25)
    expect(barPercent(300, 200)).toBe(100)
    expect(barPercent(5, 0)).toBe(0)
    expect(barPercent(Number.NaN, 10)).toBe(0)
  })

  it('finds the best value among found results only', () => {
    const r = (cost: number, found = true) => ({ cost, found }) as LabResult
    expect(bestOf([r(30), r(20), r(5, false)], (x) => x.cost)).toBe(20)
    expect(bestOf([r(5, false)], (x) => x.cost)).toBeNull()
  })

  it('has a fixed colour and label per algorithm', () => {
    expect(ALGORITHMS.map((a) => a.id)).toEqual(['bfs', 'dfs', 'ucs', 'greedy', 'astar'])
    expect(new Set(ALGORITHMS.map((a) => a.color)).size).toBe(5)
    expect(algorithmInfo('astar').short).toBe('A*')
  })
})
