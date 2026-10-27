import { useSyncExternalStore } from 'react'

export type Page = 'plan' | 'about'

function subscribe(onChange: () => void) {
  window.addEventListener('hashchange', onChange)
  return () => window.removeEventListener('hashchange', onChange)
}

function current(): Page {
  return window.location.hash.replace(/^#\/?/, '') === 'about' ? 'about' : 'plan'
}

/** The page named in the URL hash: "#/about" or anything else for the planner. */
export function useHashRoute(): Page {
  return useSyncExternalStore(subscribe, current, () => 'plan')
}
