import { ArrowUp, Mic, Sparkles, Square } from 'lucide-react'
import { useId, useState, type FormEvent } from 'react'
import { useSpeech } from '../hooks/useSpeech'

interface Props {
  onAsk: (text: string) => void
  /** Accessible name of the box. */
  label: string
  placeholder?: string
  busy?: boolean
  size?: 'md' | 'lg'
}

/** One line to type or say a trip in your own words, with a mic button where the browser supports it. */
export function AskBox({ onAsk, label, placeholder, busy = false, size = 'md' }: Props) {
  const [text, setText] = useState('')
  const inputId = useId()
  const speech = useSpeech((heard) => {
    setText('')
    onAsk(heard)
  })
  const big = size === 'lg'

  function submit(e: FormEvent) {
    e.preventDefault()
    const t = text.trim()
    if (!t || busy) return
    onAsk(t)
    setText('')
  }

  return (
    <form onSubmit={submit} aria-label={label}>
      <div
        className={`flex items-center gap-1.5 rounded-control border bg-surface pl-3 transition-[border-color,box-shadow] focus-within:border-brand focus-within:ring-2 focus-within:ring-brand/25 ${
          speech.listening ? 'border-brand ring-2 ring-brand/25' : 'border-line-strong'
        } ${big ? 'h-14 pr-1.5' : 'h-12 pr-1'}`}
      >
        <Sparkles className="size-4 shrink-0 text-brand-ink" aria-hidden="true" />
        <label htmlFor={inputId} className="sr-only">
          {label}
        </label>
        <input
          id={inputId}
          value={speech.listening ? speech.interim : text}
          onChange={(e) => setText(e.target.value)}
          readOnly={speech.listening}
          placeholder={speech.listening ? 'Listening…' : placeholder}
          maxLength={500}
          autoComplete="off"
          enterKeyHint="send"
          className={`h-full min-w-0 flex-1 bg-transparent text-ink placeholder:text-faint focus:outline-none ${big ? 'text-base' : 'text-[15px]'}`}
        />
        {speech.supported && (
          <button
            type="button"
            onClick={speech.listening ? speech.stop : speech.start}
            aria-pressed={speech.listening}
            aria-label={speech.listening ? 'Stop listening' : 'Speak your trip'}
            title={speech.listening ? 'Stop listening' : 'Speak your trip'}
            className={`grid shrink-0 place-items-center rounded-full transition-colors focus-visible:outline-2 focus-visible:outline-brand ${
              big ? 'size-11' : 'size-10'
            } ${speech.listening ? 'animate-pulse bg-danger text-white' : 'text-muted hover:bg-subtle hover:text-ink'}`}
          >
            {speech.listening ? <Square className="size-4" fill="currentColor" aria-hidden="true" /> : <Mic className="size-5" aria-hidden="true" />}
          </button>
        )}
        <button
          type="submit"
          disabled={!text.trim() || busy || speech.listening}
          aria-label="Ask"
          className={`grid shrink-0 place-items-center rounded-full bg-brand text-on-brand transition hover:bg-brand-strong focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-brand disabled:bg-subtle-2 disabled:text-faint ${
            big ? 'size-11' : 'size-10'
          }`}
        >
          {busy ? (
            <span className="size-4 animate-spin rounded-full border-2 border-current border-t-transparent" aria-hidden="true" />
          ) : (
            <ArrowUp className="size-5" aria-hidden="true" />
          )}
        </button>
      </div>
      {speech.error && (
        <p role="alert" className="mt-1.5 text-xs text-danger-ink">
          {speech.error}
        </p>
      )}
    </form>
  )
}
