import { describe, expect, it } from 'vitest'
import { clampOffset, nextSnap, settleSnap, snapOffsets } from './sheet'

describe('snapOffsets', () => {
  it('puts full near the top, half mid-way and peek a fixed height above the bottom', () => {
    expect(snapOffsets(700)).toEqual({ full: 35, half: 322, peek: 564 })
  })

  it('never lets peek rise above half on a short screen', () => {
    const o = snapOffsets(200)
    expect(o.peek).toBeGreaterThanOrEqual(o.half)
    expect(o.half).toBeGreaterThanOrEqual(o.full)
  })
})

describe('settleSnap', () => {
  const o = { full: 30, half: 320, peek: 560 }

  it('lands on the nearest snap after a slow drag', () => {
    expect(settleSnap(100, 0, o)).toBe('full')
    expect(settleSnap(300, 0.1, o)).toBe('half')
    expect(settleSnap(500, -0.1, o)).toBe('peek')
  })

  it('moves one snap in the direction of a flick', () => {
    expect(settleSnap(330, 1, o)).toBe('peek')
    expect(settleSnap(310, -1, o)).toBe('full')
    expect(settleSnap(40, 1, o)).toBe('half')
  })

  it('stays at the end when flicked past it', () => {
    expect(settleSnap(560, 2, o)).toBe('peek')
    expect(settleSnap(30, -2, o)).toBe('full')
  })
})

describe('clampOffset and nextSnap', () => {
  it('clamps a drag to the full..peek range', () => {
    const o = { full: 30, half: 320, peek: 560 }
    expect(clampOffset(-50, o)).toBe(30)
    expect(clampOffset(900, o)).toBe(560)
    expect(clampOffset(400, o)).toBe(400)
  })

  it('steps up on tap and comes back down from the top', () => {
    expect(nextSnap('peek')).toBe('half')
    expect(nextSnap('half')).toBe('full')
    expect(nextSnap('full')).toBe('half')
  })
})
