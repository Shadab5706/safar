import { CircleAlert, Sparkles } from 'lucide-react'
import { ASK_EXAMPLES } from '../lib/assistant'
import { AskBox } from './AskBox'

export interface ChatTurn {
  id: number
  who: 'user' | 'bot'
  text: string
  /** What the assistant understood: "Under ₹300", "Fastest first", ... */
  chips?: string[]
  /** Assumptions it made: 'Took "airport" as IGI Airport Terminal 3.' */
  notes?: string[]
  suggestions?: string[]
  error?: boolean
}

interface Props {
  turns: ChatTurn[]
  busy: boolean
  onAsk: (text: string) => void
}

function Chip({ children, onClick }: { children: string; onClick: () => void }) {
  return (
    <button
      type="button"
      onClick={onClick}
      className="rounded-full border border-line bg-surface px-3 py-1.5 text-left text-xs font-medium text-ink-2 shadow-card transition hover:border-brand/50 hover:text-brand-ink focus-visible:outline-2 focus-visible:outline-brand pointer-coarse:py-2"
    >
      {children}
    </button>
  )
}

/** Ask Safar: the conversation and the box to type or speak into. */
export function AssistantChat({ turns, busy, onAsk }: Props) {
  const last = turns.at(-1)
  const suggestions = !busy && last?.who === 'bot' ? (last.suggestions ?? []) : []

  return (
    <div className="space-y-3">
      {turns.length === 0 ? (
        <div className="rounded-card border border-line bg-subtle/60 p-3.5">
          <p className="flex items-start gap-2 text-sm text-ink-2">
            <Sparkles className="mt-0.5 size-4 shrink-0 text-brand-ink" aria-hidden="true" />
            <span>
              Type or say your trip in your own words. Add a budget, a time limit or what matters most, and the best match
              comes first.
            </span>
          </p>
          <ul className="mt-3 flex flex-wrap gap-1.5" aria-label="Examples">
            {ASK_EXAMPLES.map((ex) => (
              <li key={ex}>
                <Chip onClick={() => onAsk(ex)}>{ex}</Chip>
              </li>
            ))}
          </ul>
        </div>
      ) : (
        <ol role="log" aria-live="polite" aria-label="Conversation" className="space-y-2.5">
          {turns.map((t) =>
            t.who === 'user' ? (
              <li key={t.id} className="flex justify-end">
                <p className="max-w-[85%] rounded-2xl rounded-br-md bg-brand px-3.5 py-2 text-sm text-on-brand">
                  <span className="sr-only">You: </span>
                  {t.text}
                </p>
              </li>
            ) : (
              <li key={t.id} className="flex">
                <div
                  className={`max-w-[92%] rounded-2xl rounded-bl-md border px-3.5 py-2.5 text-sm ${
                    t.error ? 'border-danger-line bg-danger-soft text-danger-ink' : 'border-line bg-surface text-ink shadow-card'
                  }`}
                >
                  <span className="sr-only">Safar: </span>
                  {t.error && <CircleAlert className="mr-1.5 inline size-4 align-[-3px]" aria-hidden="true" />}
                  {t.text}
                  {t.chips && t.chips.length > 0 && (
                    <ul className="mt-2 flex flex-wrap gap-1" aria-label="Understood">
                      {t.chips.map((c) => (
                        <li key={c} className="rounded-full bg-brand-soft px-2 py-0.5 text-[11px] font-semibold text-brand-ink">
                          {c}
                        </li>
                      ))}
                    </ul>
                  )}
                  {t.notes?.map((n) => (
                    <p key={n} className="mt-1.5 text-xs text-muted">
                      {n}
                    </p>
                  ))}
                </div>
              </li>
            ),
          )}
          {busy && (
            <li className="flex" aria-label="Safar is thinking">
              <span className="flex gap-1 rounded-2xl rounded-bl-md border border-line bg-surface px-3.5 py-3 shadow-card" aria-hidden="true">
                {[0, 150, 300].map((d) => (
                  <span key={d} className="size-1.5 animate-bounce rounded-full bg-muted" style={{ animationDelay: `${d}ms` }} />
                ))}
              </span>
            </li>
          )}
        </ol>
      )}

      <AskBox
        onAsk={onAsk}
        busy={busy}
        label="Ask Safar"
        placeholder={turns.length ? 'Ask a follow-up, like "under ₹200"' : 'e.g. CP to the airport under ₹200'}
      />

      {suggestions.length > 0 && (
        <ul className="flex flex-wrap gap-1.5" aria-label="Suggestions">
          {suggestions.map((s) => (
            <li key={s}>
              <Chip onClick={() => onAsk(s)}>{s}</Chip>
            </li>
          ))}
        </ul>
      )}
    </div>
  )
}
