import { useEffect, useState } from 'react'

export const SPEEDS = [
  { rate: 10, label: 'Slow' },
  { rate: 40, label: 'Normal' },
  { rate: 120, label: 'Fast' },
  { rate: 400, label: 'Very fast' },
] as const

export interface Player {
  step: number
  playing: boolean // true while the animation is actually advancing
  rate: number
  setRate: (rate: number) => void
  play: () => void
  pause: () => void
  seek: (step: number) => void
  restart: () => void
}

/** Steps through `length` frames at `rate` per second. Starts paused at step 0. */
export function usePlayer(length: number): Player {
  const [step, setStep] = useState(0)
  const [wantPlay, setWantPlay] = useState(false)
  const [rate, setRate] = useState<number>(SPEEDS[1].rate)
  const lastStep = Math.max(0, length - 1)
  const clamped = Math.min(step, lastStep)
  const playing = wantPlay && clamped < lastStep

  useEffect(() => {
    if (!playing) return
    const tick = Math.max(16, 1000 / rate)
    const inc = Math.max(1, Math.round((rate * tick) / 1000))
    const id = window.setInterval(() => setStep((s) => Math.min(lastStep, s + inc)), tick)
    return () => window.clearInterval(id)
  }, [playing, rate, lastStep])

  return {
    step: clamped,
    playing,
    rate,
    setRate,
    play: () => {
      if (clamped >= lastStep) setStep(0)
      setWantPlay(true)
    },
    pause: () => setWantPlay(false),
    seek: (s) => {
      setWantPlay(false)
      setStep(Math.max(0, Math.min(lastStep, s)))
    },
    restart: () => {
      setStep(0)
      setWantPlay(true)
    },
  }
}
