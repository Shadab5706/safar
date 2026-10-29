# Turning Safar into a mobile app

The web frontend is built to be wrapped as an Android and iOS app without changes:

- **Mobile-first layout.** Bottom tab bar, a draggable results sheet over the map, touch targets of
  at least 44 px and safe-area padding for notched phones.
- **Installable already.** `manifest.webmanifest`, app icons and a service worker that caches the
  app shell (never API answers). Chrome on Android shows *Install app*; on iPhone use Safari's
  Share → *Add to Home Screen*.
- **One place for network calls.** Every request goes through `frontend/src/api/client.ts`, and the
  API root comes from `VITE_API_BASE` at build time (default `/api/v1`, the website's own proxy).
- **Hash routes** (`#/plan`, `#/lab`…), so the app works from any local file server, which is what a
  native wrapper uses.

## Wrap it with Capacitor

[Capacitor](https://capacitorjs.com) puts the built site in a native WebView. You need Android
Studio for Android, and a Mac with Xcode for iOS.

```bash
cd frontend
npm install @capacitor/core @capacitor/cli @capacitor/android @capacitor/ios
npx cap init Safar in.safar.app --web-dir dist
```

Then set `frontend/capacitor.config.ts` to:

```ts
import type { CapacitorConfig } from '@capacitor/cli'

const config: CapacitorConfig = {
  appId: 'in.safar.app',
  appName: 'Safar',
  webDir: 'dist',
  // Send fetch() through native HTTP: the app's WebView origin (https://localhost on Android,
  // capacitor://localhost on iOS) is not the API's allowed CORS origin.
  plugins: { CapacitorHttp: { enabled: true } },
}

export default config
```

Build against the live API and open the native project:

```bash
VITE_API_BASE=https://safar-delhi-api.onrender.com/api/v1 npm run build
npx cap add android        # once; `npx cap add ios` on a Mac
npx cap sync
npx cap open android       # builds and runs from Android Studio
```

Run `npm run build` and `npx cap sync` again after every frontend change.

## Things to know

- **Waking the server.** With an absolute `VITE_API_BASE` the app pings `<API>/health` directly to
  wake a sleeping free-tier server, and shows the same "waking up the server" notice as the site.
- **Map tiles.** The app uses the public OpenStreetMap tile servers, which are fine for a course
  demo but not for a published app with real traffic. Before a store release, switch the
  `TileLayer` URL in `src/components/BaseMap.tsx` to a tile provider with a free key (MapTiler,
  Stadia Maps, Thunderforest…).
- **Booking links** (Uber, Ola, Rapido) open outside the app, so they reach the installed ride app
  the same way they do from a phone browser.
- **Icons and splash screens.** Generate the native sets from `public/icons/icon-512.png` with
  `npx @capacitor/assets generate`.
- **Service worker.** Android's WebView runs it; iOS skips it. Either way the app works, because the
  native wrapper already ships the files.
- **Stores.** Google Play needs a one-time developer fee; the App Store needs a yearly Apple
  Developer membership.
