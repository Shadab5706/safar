import { useSyncExternalStore } from 'react'
import { isWaking, subscribeWaking } from '../api/client'

/** A notice shown while requests wait for the free-tier server to wake up. */
export function ServerWaking() {
  const waking = useSyncExternalStore(subscribeWaking, isWaking)
  return (
    <div role="status" aria-live="polite" className="pointer-events-none fixed inset-x-0 top-[calc(var(--bar-h)+env(safe-area-inset-top)+0.5rem)] z-[1600] flex justify-center px-4">
      {waking && (
        <p className="flex max-w-md animate-rise items-start gap-2.5 rounded-2xl border border-warn-line bg-warn-soft px-4 py-2.5 text-sm text-warn-ink shadow-pop">
          <span className="mt-0.5 size-4 shrink-0 animate-spin rounded-full border-2 border-warn-ink border-t-transparent" aria-hidden="true" />
          <span>
            <strong className="font-semibold">Waking up the server.</strong> Free hosting sleeps when nobody has used it for a
            while, so this can take up to a minute.
          </span>
        </p>
      )}
    </div>
  )
}
