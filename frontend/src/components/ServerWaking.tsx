import { useSyncExternalStore } from 'react'
import { isWaking, subscribeWaking } from '../api/client'

/** A banner shown while requests wait for the free-tier server to wake up. */
export function ServerWaking() {
  const waking = useSyncExternalStore(subscribeWaking, isWaking)
  return (
    <div role="status" aria-live="polite">
      {waking && (
        <p className="flex items-center justify-center gap-2 border-b border-amber-200 bg-amber-50 px-4 py-2 text-sm text-amber-900">
          <span className="size-3.5 animate-spin rounded-full border-2 border-amber-700 border-t-transparent" aria-hidden="true" />
          Waking up the server. Free hosting sleeps when nobody has used it for a while, so this can take up to a minute.
        </p>
      )}
    </div>
  )
}
