import { useCallback, useEffect, useRef, useState } from 'react'
import { speechErrorMessage, speechRecognitionCtor, transcriptOf, type SpeechRecognitionLike } from '../lib/assistant'

export interface Speech {
  supported: boolean
  listening: boolean
  /** What has been heard so far while listening. */
  interim: string
  error: string | null
  start: () => void
  stop: () => void
}

/**
 * Speech to text with the browser's own recogniser (Indian English). Calls `onHeard` once
 * with the whole sentence when the speaker pauses.
 */
export function useSpeech(onHeard: (text: string) => void): Speech {
  const [Ctor] = useState(() => speechRecognitionCtor(typeof window === 'undefined' ? null : window))
  const [listening, setListening] = useState(false)
  const [interim, setInterim] = useState('')
  const [error, setError] = useState<string | null>(null)
  const recognition = useRef<SpeechRecognitionLike | null>(null)
  const heardRef = useRef(onHeard)

  useEffect(() => {
    heardRef.current = onHeard
  })

  // Leaving the page mid-sentence: stop without sending anything.
  useEffect(
    () => () => {
      const r = recognition.current
      if (r) {
        r.onend = null
        r.abort()
      }
    },
    [],
  )

  const start = useCallback(() => {
    if (!Ctor) return
    recognition.current?.abort()
    const r = new Ctor()
    r.lang = 'en-IN'
    r.interimResults = true
    r.continuous = false
    r.maxAlternatives = 1
    let heard = ''
    r.onresult = (e) => {
      heard = transcriptOf(e.results).text
      setInterim(heard)
    }
    r.onerror = (e) => setError(speechErrorMessage(e.error))
    r.onend = () => {
      recognition.current = null
      setListening(false)
      setInterim('')
      if (heard) heardRef.current(heard)
    }
    setError(null)
    setInterim('')
    try {
      r.start()
      recognition.current = r
      setListening(true)
    } catch {
      setError('Could not start the microphone. You can type instead.')
    }
  }, [Ctor])

  const stop = useCallback(() => recognition.current?.stop(), [])

  return { supported: Ctor !== null, listening, interim, error, start, stop }
}
