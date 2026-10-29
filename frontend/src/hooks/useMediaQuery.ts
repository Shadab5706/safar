import { useCallback, useSyncExternalStore } from 'react'

/** True while the CSS media query matches. */
export function useMediaQuery(query: string): boolean {
  const subscribe = useCallback(
    (onChange: () => void) => {
      const mql = window.matchMedia(query)
      mql.addEventListener('change', onChange)
      return () => mql.removeEventListener('change', onChange)
    },
    [query],
  )
  return useSyncExternalStore(subscribe, () => window.matchMedia(query).matches, () => false)
}

/** Large screens get the side panel and top navigation; smaller ones the bottom sheet. */
export const DESKTOP = '(min-width: 1024px)'
