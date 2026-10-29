/// <reference types="vitest/config" />
import tailwindcss from '@tailwindcss/vite'
import react from '@vitejs/plugin-react'
import { createHash } from 'node:crypto'
import { readFileSync } from 'node:fs'
import { defineConfig, type Plugin } from 'vite'

// The C++ backend (backend/build/safar_server) listens on :8080 by default.
const apiTarget = process.env.SAFAR_API ?? 'http://localhost:8080'

// Files in public/ that belong to the app shell (the built JS, CSS and fonts are added from the bundle).
const PUBLIC_SHELL = ['manifest.webmanifest', 'favicon.svg', 'icons/icon-192.png', 'icons/icon-512.png', 'icons/apple-touch-icon.png']

/** Emits sw.js with the list of files to pre-cache, so the installed app opens offline. */
function serviceWorker(): Plugin {
  return {
    name: 'safar-service-worker',
    apply: 'build',
    generateBundle(_, bundle) {
      // Fonts for other scripts are fetched only if a page needs them; pre-cache Latin only.
      const built = Object.keys(bundle).filter(
        (f) => !f.endsWith('.map') && f !== 'index.html' && (!f.endsWith('.woff2') || /-latin-(ext-)?wght-/.test(f)),
      )
      const version = createHash('sha256').update(built.sort().join('\n')).digest('hex').slice(0, 12)
      const shell = ['./', ...[...built, ...PUBLIC_SHELL].map((f) => `./${f}`)]
      const source = readFileSync(new URL('./pwa/sw.js', import.meta.url), 'utf8')
        .replace('__VERSION__', version)
        .replace('__SHELL__', JSON.stringify(shell))
      this.emitFile({ type: 'asset', fileName: 'sw.js', source })
    },
  }
}

export default defineConfig({
  plugins: [react(), tailwindcss(), serviceWorker()],
  server: {
    port: 5173,
    proxy: { '/api': { target: apiTarget, changeOrigin: true } },
  },
  preview: {
    proxy: { '/api': { target: apiTarget, changeOrigin: true } },
  },
  test: {
    environment: 'node',
    include: ['src/**/*.test.ts'],
  },
})
