// Safar service worker. It keeps the app shell (HTML, scripts, styles, fonts, icons) so the app
// opens instantly and offline. API responses are never cached: routes and prices must be fresh.
// The build fills in VERSION and SHELL (see vite.config.ts).
const VERSION = '__VERSION__'
const SHELL = __SHELL__
const CACHE = `safar-shell-${VERSION}`

self.addEventListener('install', (event) => {
  event.waitUntil(
    caches
      .open(CACHE)
      .then((cache) => cache.addAll(SHELL))
      .then(() => self.skipWaiting()),
  )
})

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches
      .keys()
      .then((keys) => Promise.all(keys.filter((k) => k.startsWith('safar-shell-') && k !== CACHE).map((k) => caches.delete(k))))
      .then(() => self.clients.claim()),
  )
})

self.addEventListener('fetch', (event) => {
  const req = event.request
  if (req.method !== 'GET') return
  const url = new URL(req.url)
  // Only this site's own files: never the API, the wake-up address or the map tiles.
  if (url.origin !== self.location.origin) return
  if (url.pathname.includes('/api/') || url.pathname.endsWith('/wake.json')) return

  if (req.mode === 'navigate') {
    // Network first, so a new release shows up at once; the cached page when offline.
    event.respondWith(fetch(req).catch(() => caches.match('./', { ignoreSearch: true }).then((hit) => hit ?? Response.error())))
    return
  }
  event.respondWith(caches.match(req).then((hit) => hit ?? fetch(req)))
})
