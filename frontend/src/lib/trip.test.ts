import { describe, expect, it } from 'vitest'
import type { CurvePoint, Endpoint } from '../api/types'
import { linePath, linearScale, moveItem, niceTicks, orderNames, pointAt, savings } from './trip'

describe('linearScale', () => {
  it('maps a domain onto a range, including inverted ranges', () => {
    const y = linearScale([0, 100], [200, 0])
    expect(y(0)).toBe(200)
    expect(y(50)).toBe(100)
    expect(linearScale([5, 5], [0, 10])(5)).toBe(5)
  })
})

describe('niceTicks', () => {
  it('picks round steps', () => {
    expect(niceTicks(0, 100, 4)).toEqual([0, 20, 40, 60, 80, 100])
    expect(niceTicks(612, 838, 4)).toEqual([650, 700, 750, 800])
    expect(niceTicks(0, 4000, 4)).toEqual([0, 1000, 2000, 3000, 4000])
    expect(niceTicks(3, 3)).toEqual([3])
  })
})

describe('linePath', () => {
  const pts: CurvePoint[] = [
    { evaluations: 0, current: 10, best: 10 },
    { evaluations: 5, current: 12, best: 10 },
    { evaluations: 10, current: 6, best: 6 },
  ]
  const id = (v: number) => v

  it('draws straight segments or a step function', () => {
    expect(linePath(pts, 'current', id, id)).toBe('M0.0,10.0L5.0,12.0L10.0,6.0')
    expect(linePath(pts, 'best', id, id, true)).toBe('M0.0,10.0H5.0V10.0H10.0V6.0')
    expect(linePath([], 'best', id, id)).toBe('')
  })

  it('reads the point at or before an x value', () => {
    expect(pointAt(pts, 7)?.evaluations).toBe(5)
    expect(pointAt(pts, -1)).toBeNull()
    expect(pointAt(pts, 99)?.best).toBe(6)
  })
})

describe('trip helpers', () => {
  const stop = (name: string): Endpoint => ({ id: name, name, type: 'place', lat: 0, lng: 0 })
  const stops = [stop('A'), stop('B'), stop('C')]

  it('lists stops in visiting order', () => {
    expect(orderNames([0, 2, 1], stops, false)).toEqual(['A', 'C', 'B'])
    expect(orderNames([0, 2, 1], stops, true)).toEqual(['A', 'C', 'B', 'A'])
  })

  it('computes what the better order saves', () => {
    const input = { order: [0, 1, 2], score: 200, total_min: 150, fare_rs: 300 }
    const best = { order: [0, 2, 1], score: 150, total_min: 110, fare_rs: 280 }
    expect(savings(input, best)).toEqual({ min: 40, rs: 20, pct: 25 })
  })

  it('moves list items', () => {
    expect(moveItem(['a', 'b', 'c'], 2, 0)).toEqual(['c', 'a', 'b'])
    expect(moveItem(['a', 'b', 'c'], 0, 5)).toEqual(['a', 'b', 'c'])
  })
})
