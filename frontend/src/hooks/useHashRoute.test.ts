import { describe, expect, it } from 'vitest'
import { hrefFor, pageFromHash } from './useHashRoute'

describe('pageFromHash', () => {
  it('opens home for an empty or unknown hash', () => {
    expect(pageFromHash('')).toBe('home')
    expect(pageFromHash('#/')).toBe('home')
    expect(pageFromHash('#/nowhere')).toBe('home')
  })

  it('reads the page name, ignoring anything after it', () => {
    expect(pageFromHash('#/plan')).toBe('plan')
    expect(pageFromHash('#/lab/extra')).toBe('lab')
    expect(pageFromHash('#/trips?x=1')).toBe('trips')
    expect(pageFromHash('#about')).toBe('about')
  })

  it('round-trips through hrefFor', () => {
    for (const p of ['home', 'plan', 'lab', 'trips', 'about'] as const) expect(pageFromHash(hrefFor(p))).toBe(p)
  })
})
