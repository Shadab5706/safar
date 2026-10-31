import { describe, expect, it } from 'vitest'
import type { Understood } from '../api/types'
import type { MetroNetwork } from '../hooks/useMetroNetwork'
import {
  departLabel,
  endpointToResult,
  preferenceFor,
  speechErrorMessage,
  speechRecognitionCtor,
  transcriptOf,
  understoodChips,
} from './assistant'

const base: Understood = {
  from: null,
  to: null,
  objective: 'balanced',
  label: 'balanced',
  confidence: 0.6,
  budget_rs: null,
  max_minutes: null,
  depart_at: null,
  modes: ['walk', 'metro', 'cab', 'auto', 'bike'],
  needs: [],
  notes: [],
}
const now = new Date(2026, 9, 31, 9, 0) // Saturday 31 Oct 2026, 09:00

describe('understoodChips', () => {
  it('says nothing for a plain trip', () => {
    expect(understoodChips(base, now)).toEqual([])
  })

  it('lists the objective, budget, time limit and departure', () => {
    const chips = understoodChips({ ...base, objective: 'cheapest', budget_rs: 300, max_minutes: 90, depart_at: '2026-11-01T18:00' }, now)
    expect(chips).toEqual(['Cheapest first', 'Under ₹300', 'Within 1 h 30 min', 'Leave tomorrow 18:00'])
  })

  it('names restricted, excluded and required modes', () => {
    expect(understoodChips({ ...base, modes: ['walk', 'metro'] }, now)).toEqual(['Only Metro'])
    expect(understoodChips({ ...base, modes: ['walk', 'metro', 'auto', 'bike'] }, now)).toEqual(['No cab'])
    expect(understoodChips({ ...base, needs: ['metro'] }, now)).toEqual(['By metro'])
  })
})

describe('departLabel', () => {
  it('shows the day only when it is not today', () => {
    expect(departLabel('2026-10-31T18:30', now)).toBe('Leave 18:30')
    expect(departLabel('2026-11-01T09:00', now)).toBe('Leave tomorrow 09:00')
    expect(departLabel('2026-11-03T09:00', now)).toBe('Leave Tue 09:00')
    expect(departLabel('nonsense', now)).toBe('')
  })
})

describe('endpointToResult', () => {
  const network = { lines: [], stations: [{ id: 'hauz-khas', name: 'Hauz Khas', lat: 28.5, lng: 77.2, lines: ['yellow', 'magenta'] }], lineById: new Map() } as MetroNetwork

  it('fills in the lines of a station from the network', () => {
    expect(endpointToResult({ type: 'station', id: 'hauz-khas', name: 'Hauz Khas', lat: 28.5, lng: 77.2 }, network)).toEqual({
      kind: 'station',
      id: 'hauz-khas',
      name: 'Hauz Khas',
      lat: 28.5,
      lng: 77.2,
      lines: ['yellow', 'magenta'],
    })
  })

  it('keeps places as places, with or without the network', () => {
    expect(endpointToResult({ type: 'place', id: 'india-gate', name: 'India Gate', lat: 28.6, lng: 77.2 }, null)).toMatchObject({ kind: 'place', id: 'india-gate' })
    expect(endpointToResult({ type: 'station', id: 'x', name: 'X', lat: 0, lng: 0 }, null)).toMatchObject({ lines: [] })
  })
})

describe('preferenceFor', () => {
  it('maps the objective onto the cheapest … fastest slider', () => {
    expect(preferenceFor('cheapest')).toBe(0)
    expect(preferenceFor('fastest')).toBe(1)
    expect(preferenceFor('comfort')).toBe(0.5)
  })
})

describe('voice input helpers', () => {
  it('finds the standard or the webkit recogniser', () => {
    class Rec {}
    expect(speechRecognitionCtor({ SpeechRecognition: Rec })).toBe(Rec)
    expect(speechRecognitionCtor({ webkitSpeechRecognition: Rec })).toBe(Rec)
    expect(speechRecognitionCtor({})).toBeNull()
    expect(speechRecognitionCtor(null)).toBeNull()
  })

  it('joins partial results and knows when they are final', () => {
    const results = { length: 2, 0: { isFinal: true, 0: { transcript: 'cp to ' } }, 1: { isFinal: false, 0: { transcript: 'airport ' } } }
    expect(transcriptOf(results)).toEqual({ text: 'cp to airport', final: false })
    expect(transcriptOf({ length: 0 })).toEqual({ text: '', final: false })
  })

  it('explains errors, and stays quiet when the user stopped it', () => {
    expect(speechErrorMessage('aborted')).toBeNull()
    expect(speechErrorMessage('not-allowed')).toMatch(/blocked/)
    expect(speechErrorMessage('something-new')).toMatch(/type instead/)
  })
})
