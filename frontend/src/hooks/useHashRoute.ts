import { useSyncExternalStore } from 'react'

export type Page = 'plan' | 'lab' | 'trips' | 'about'

const PAGES: readonly Page[] = ['plan', 'lab', 'trips', 'about']

function subscribe(onChange: () => void) {
  window.addEventListener('hashchange', onChange)
  return () => window.removeEventListener('hashchange', onChange)
}

function current(): Page {
  const name = window.location.hash.replace(/^#\/?/, '').split(/[/?]/)[0]
  return PAGES.find((p) => p === name) ?? 'plan'
}

/** The page named in the URL hash: "#/lab", "#/trips", "#/about", or anything else for the planner. */
export function useHashRoute(): Page {
  return useSyncExternalStore(subscribe, current, () => 'plan')
}
