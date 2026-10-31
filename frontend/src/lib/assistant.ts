import type { Endpoint, Mode, Objective, SearchResult, Understood } from '../api/types'
import type { MetroNetwork } from '../hooks/useMetroNetwork'
import { formatClock, formatDuration, formatRupees, modeLabel, parseLocal } from './format'

/** Things to try, shown before the first message. */
export const ASK_EXAMPLES: readonly string[] = [
  'Bennett University to Hauz Khas Village under ₹300',
  'Fastest way from CP to the airport',
  'Rajiv Chowk to Cyber Hub within 1 hour',
  'Noida Sector 18 se India Gate sabse sasta',
]

/** A place or station from the assistant, in the autocomplete's shape, so the form can show it. */
export function endpointToResult(e: Endpoint, network: MetroNetwork | null): SearchResult {
  if (e.type === 'station') {
    const lines = network?.stations.find((s) => s.id === e.id)?.lines ?? []
    return { kind: 'station', id: e.id, name: e.name, lat: e.lat, lng: e.lng, lines }
  }
  return { kind: 'place', id: e.id, name: e.name, lat: e.lat, lng: e.lng }
}

/** Where the form's cheapest … fastest slider sits for what the traveller asked for. */
export function preferenceFor(objective: Objective): number {
  if (objective === 'cheapest') return 0
  if (objective === 'fastest') return 1
  return 0.5
}

const OBJECTIVES: Record<Objective, string> = {
  balanced: 'Best balance',
  cheapest: 'Cheapest first',
  fastest: 'Fastest first',
  fewest_changes: 'Fewest changes',
  comfort: 'Least walking',
}

const DAYS = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat']

/** "Leave 18:00", "Leave tomorrow 09:00", "Leave Sun 10:30". */
export function departLabel(departAt: string, now: Date): string {
  const d = parseLocal(departAt)
  if (!d) return ''
  const start = new Date(now.getFullYear(), now.getMonth(), now.getDate())
  const days = Math.round((new Date(d.getFullYear(), d.getMonth(), d.getDate()).getTime() - start.getTime()) / 86_400_000)
  const day = days === 0 ? '' : days === 1 ? 'tomorrow ' : `${DAYS[d.getDay()]} `
  return `Leave ${day}${formatClock(departAt)}`
}

const RIDES: readonly Mode[] = ['metro', 'cab', 'auto', 'bike']

/** Short labels for what the assistant understood: "Under ₹300", "Within 2 h", "Only Metro", "No cab". */
export function understoodChips(u: Understood, now: Date): string[] {
  const chips: string[] = []
  if (u.objective !== 'balanced') chips.push(OBJECTIVES[u.objective])
  if (u.budget_rs != null) chips.push(`Under ${formatRupees(u.budget_rs)}`)
  if (u.max_minutes != null) chips.push(`Within ${formatDuration(u.max_minutes)}`)
  if (u.depart_at) chips.push(departLabel(u.depart_at, now))
  const allowed = RIDES.filter((m) => u.modes.includes(m))
  if (allowed.length > 0 && allowed.length <= 2) {
    chips.push(`Only ${allowed.map(modeLabel).join(' + ')}`)
  } else {
    for (const m of RIDES) if (!allowed.includes(m)) chips.push(`No ${modeLabel(m).toLowerCase()}`)
  }
  for (const m of u.needs) chips.push(`By ${modeLabel(m).toLowerCase()}`)
  return chips
}

// ---- Voice input (Web Speech API: Chrome, Edge, Safari) ----

export interface SpeechResultList {
  length: number
  [index: number]: { isFinal: boolean; 0: { transcript: string } }
}

export interface SpeechRecognitionLike {
  lang: string
  interimResults: boolean
  continuous: boolean
  maxAlternatives: number
  start(): void
  stop(): void
  abort(): void
  onresult: ((e: { results: SpeechResultList }) => void) | null
  onerror: ((e: { error: string }) => void) | null
  onend: (() => void) | null
}

export type SpeechRecognitionCtor = new () => SpeechRecognitionLike

/** The browser's speech recogniser, or null where there is none (Firefox, most webviews). */
export function speechRecognitionCtor(win: unknown): SpeechRecognitionCtor | null {
  if (!win || typeof win !== 'object') return null
  const w = win as { SpeechRecognition?: unknown; webkitSpeechRecognition?: unknown }
  const ctor = w.SpeechRecognition ?? w.webkitSpeechRecognition
  return typeof ctor === 'function' ? (ctor as SpeechRecognitionCtor) : null
}

/** Everything heard so far, and whether the recogniser has settled on it. */
export function transcriptOf(results: SpeechResultList): { text: string; final: boolean } {
  let text = ''
  let final = true
  for (let i = 0; i < results.length; i++) {
    text += results[i]![0].transcript
    final &&= results[i]!.isFinal
  }
  return { text: text.trim(), final: results.length > 0 && final }
}

/** A friendly message for a recogniser error code, or null when nothing needs saying. */
export function speechErrorMessage(code: string): string | null {
  switch (code) {
    case 'aborted':
      return null
    case 'not-allowed':
    case 'service-not-allowed':
      return 'Microphone access is blocked. Allow it in your browser to speak, or type instead.'
    case 'no-speech':
      return "I didn't hear anything. Tap the mic and try again."
    case 'audio-capture':
      return 'No microphone was found. You can type instead.'
    case 'network':
      return 'Voice input needs an internet connection. You can type instead.'
    default:
      return 'Voice input stopped. You can type instead.'
  }
}
