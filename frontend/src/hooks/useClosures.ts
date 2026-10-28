import { useCallback, useEffect, useMemo, useState } from 'react'
import { addClosure, ApiError, clearClosures, getClosures, isAbortError, removeClosure } from '../api/client'
import type { Closure, ClosureList, ClosureRequest } from '../api/types'
import { closedSets, type ClosedSets } from '../lib/closures'

export interface ClosuresApi {
  status: 'loading' | 'ready' | 'error'
  items: Closure[]
  /** Server-side version; changes whenever anything is closed or reopened. -1 until loaded. */
  version: number
  sets: ClosedSets
  pending: boolean
  error: string | null
  close: (req: ClosureRequest) => void
  reopen: (id: string) => void
  reopenAll: () => void
  reload: () => void
  dismissError: () => void
}

const message = (err: unknown) => (err instanceof ApiError ? err.message : 'Something went wrong. Please try again.')

/** The server's list of line closures, shared by every screen that plans routes. */
export function useClosures(): ClosuresApi {
  const [list, setList] = useState<ClosureList>({ items: [], version: -1 })
  const [status, setStatus] = useState<ClosuresApi['status']>('loading')
  const [pending, setPending] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [attempt, setAttempt] = useState(0)

  useEffect(() => {
    const ctl = new AbortController()
    getClosures(ctl.signal)
      .then((l) => {
        setList(l)
        setStatus('ready')
      })
      .catch((err: unknown) => {
        if (isAbortError(err)) return
        setStatus('error')
        setError(message(err))
      })
    return () => ctl.abort()
  }, [attempt])

  const act = useCallback((run: () => Promise<ClosureList>) => {
    setPending(true)
    setError(null)
    run()
      .then((l) => {
        setList({ items: l.items, version: l.version })
        setStatus('ready')
      })
      .catch((err: unknown) => setError(message(err)))
      .finally(() => setPending(false))
  }, [])

  const close = useCallback((req: ClosureRequest) => act(() => addClosure(req)), [act])
  const reopen = useCallback((id: string) => act(() => removeClosure(id)), [act])
  const reopenAll = useCallback(() => act(clearClosures), [act])
  const reload = useCallback(() => {
    setStatus('loading')
    setError(null)
    setAttempt((n) => n + 1)
  }, [])
  const dismissError = useCallback(() => setError(null), [])
  const sets = useMemo(() => closedSets(list.items), [list.items])

  return { status, items: list.items, version: list.version, sets, pending, error, close, reopen, reopenAll, reload, dismissError }
}
