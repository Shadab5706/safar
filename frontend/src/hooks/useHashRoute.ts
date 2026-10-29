import { useSyncExternalStore } from 'react'

export type Page = 'home' | 'plan' | 'lab' | 'trips' | 'about'

const PAGES: readonly Page[] = ['home', 'plan', 'lab', 'trips', 'about']

function subscribe(onChange: () => void) {
  window.addEventListener('hashchange', onChange)
  return () => window.removeEventListener('hashchange', onChange)
}

/** The page a URL hash names: "#/plan", "#/lab", "#/trips", "#/about", or anything else for home. */
export function pageFromHash(hash: string): Page {
  const name = hash.replace(/^#\/?/, '').split(/[/?]/)[0]
  return PAGES.find((p) => p === name) ?? 'home'
}

export function useHashRoute(): Page {
  return useSyncExternalStore(subscribe, () => pageFromHash(window.location.hash), () => 'home')
}

export function hrefFor(page: Page): string {
  return page === 'home' ? '#/' : `#/${page}`
}
