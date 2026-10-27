import { describe, expect, it } from 'vitest'
import {
  formatChanges,
  formatClock,
  formatDistance,
  formatDuration,
  formatRupeeRange,
  formatRupees,
  nextQuarterHour,
  parseLocal,
  tagLabel,
  toLocalInputValue,
  zoneLabel,
} from './format'

describe('formatDuration', () => {
  it('shows minutes under an hour', () => {
    expect(formatDuration(45)).toBe('45 min')
    expect(formatDuration(45.6)).toBe('46 min')
  })
  it('shows hours and minutes', () => {
    expect(formatDuration(154.7)).toBe('2 h 35 min')
    expect(formatDuration(60)).toBe('1 h')
    expect(formatDuration(119.6)).toBe('2 h')
  })
  it('never shows 0 min for a real step', () => {
    expect(formatDuration(0.3)).toBe('1 min')
    expect(formatDuration(0)).toBe('0 min')
    expect(formatDuration(Number.NaN)).toBe('0 min')
  })
})

describe('money', () => {
  it('uses the rupee sign and Indian digit grouping', () => {
    expect(formatRupees(223)).toBe('₹223')
    expect(formatRupees(872.4)).toBe('₹872')
    expect(formatRupees(123456)).toBe('₹1,23,456')
  })
  it('formats a fare range', () => {
    expect(formatRupeeRange([97, 131])).toBe('₹97–131')
    expect(formatRupeeRange([50, 50.2])).toBe('₹50')
  })
})

describe('formatDistance', () => {
  it('switches between metres and kilometres', () => {
    expect(formatDistance(0.34)).toBe('340 m')
    expect(formatDistance(62.94)).toBe('62.9 km')
    expect(formatDistance(123.4)).toBe('123 km')
    expect(formatDistance(0)).toBe('0 m')
  })
})

describe('labels', () => {
  it('counts changes', () => {
    expect(formatChanges(0)).toBe('No changes')
    expect(formatChanges(1)).toBe('1 change')
    expect(formatChanges(3)).toBe('3 changes')
  })
  it('names tags and ticket zones', () => {
    expect(tagLabel('fewest_changes')).toBe('Fewest changes')
    expect(tagLabel('something_new')).toBe('something new')
    expect(zoneLabel('NMRC')).toBe('Noida Metro (NMRC)')
    expect(zoneLabel('XYZ')).toBe('XYZ')
  })
})

describe('local times', () => {
  it('round-trips a datetime-local value without touching UTC', () => {
    const d = new Date(2026, 9, 27, 9, 5)
    expect(toLocalInputValue(d)).toBe('2026-10-27T09:05')
    expect(parseLocal('2026-10-27T09:05')?.getTime()).toBe(d.getTime())
    expect(parseLocal('not a date')).toBeNull()
  })
  it('rounds up to the next quarter hour', () => {
    expect(toLocalInputValue(nextQuarterHour(new Date(2026, 9, 27, 9, 1, 30)))).toBe('2026-10-27T09:15')
    expect(toLocalInputValue(nextQuarterHour(new Date(2026, 9, 27, 9, 45)))).toBe('2026-10-27T09:45')
    expect(toLocalInputValue(nextQuarterHour(new Date(2026, 9, 27, 23, 50)))).toBe('2026-10-28T00:00')
  })
  it('adds trip minutes to the departure clock', () => {
    expect(formatClock('2026-10-27T09:00')).toBe('09:00')
    expect(formatClock('2026-10-27T09:00', 154.7)).toBe('11:35')
    expect(formatClock('2026-10-27T23:30', 45)).toBe('00:15')
    expect(formatClock('')).toBe('')
  })
})
