import { Download, FlaskConical, House, Info, Navigation, Route as RouteIcon, type LucideIcon } from 'lucide-react'
import { lazy, Suspense, useEffect, useRef, useState, type ReactNode } from 'react'
import { getHealth, wakeApi } from './api/client'
import type { SearchResult } from './api/types'
import { ServerWaking } from './components/ServerWaking'
import { useClosures } from './hooks/useClosures'
import { hrefFor, useHashRoute, type Page } from './hooks/useHashRoute'
import { useInstallPrompt } from './hooks/useInstallPrompt'
import { useMetroNetwork } from './hooks/useMetroNetwork'
import type { ExampleTrip } from './lib/examples'
import type { PlanIntent } from './lib/query'
import { HomePage } from './pages/HomePage'
import { PlanPage } from './pages/PlanPage'

// Screens off the main path load on first visit, so the app starts faster on a phone.
const LabPage = lazy(() => import('./pages/LabPage').then((m) => ({ default: m.LabPage })))
const TripPage = lazy(() => import('./pages/TripPage').then((m) => ({ default: m.TripPage })))
const AboutPage = lazy(() => import('./pages/AboutPage').then((m) => ({ default: m.AboutPage })))

const TABS: { page: Page; label: string; short?: string; icon: LucideIcon }[] = [
  { page: 'home', label: 'Home', icon: House },
  { page: 'plan', label: 'Plan', icon: Navigation },
  { page: 'lab', label: 'Algorithm Lab', short: 'Lab', icon: FlaskConical },
  { page: 'trips', label: 'Trip planner', short: 'Trips', icon: RouteIcon },
]

const MAP_PAGES: readonly Page[] = ['plan', 'lab', 'trips']

function Loading({ children }: { children: ReactNode }) {
  return (
    <Suspense
      fallback={
        <div className="grid h-full min-h-60 place-items-center" role="status">
          <span className="size-6 animate-spin rounded-full border-2 border-brand border-t-transparent" aria-hidden="true" />
          <span className="sr-only">Loading…</span>
        </div>
      }
    >
      {children}
    </Suspense>
  )
}

function Logo() {
  return (
    <svg viewBox="0 0 32 32" className="size-8" aria-hidden="true">
      <rect width="32" height="32" rx="9" fill="#0d7064" />
      <path d="M9 22c3-9 11-3 14-12" stroke="#fff" strokeWidth="3" fill="none" strokeLinecap="round" />
      <circle cx="9" cy="22" r="3" fill="#f2a516" />
      <circle cx="23" cy="10" r="3" fill="#fff" />
    </svg>
  )
}

export default function App() {
  const page = useHashRoute()
  const network = useMetroNetwork()
  const closures = useClosures()
  const install = useInstallPrompt()
  const [intent, setIntent] = useState<PlanIntent | null>(null)
  const nextIntent = useRef(1)
  const main = useRef<HTMLElement>(null)
  // Map screens mount on first visit and then stay mounted, so switching tabs keeps their state.
  const [visited, setVisited] = useState<ReadonlySet<Page>>(() => new Set([page]))
  if (!visited.has(page)) setVisited(new Set([...visited, page]))

  // Start a sleeping server as soon as the page opens, so it is up by the time a search is sent.
  useEffect(() => {
    void wakeApi()
    getHealth().catch(() => {})
  }, [])

  // Scrolling pages share one scroller: start each one at the top.
  useEffect(() => {
    main.current?.scrollTo({ top: 0 })
  }, [page])

  function startPlan(next: { from: SearchResult | null; to: SearchResult | null } | { example: ExampleTrip }) {
    setIntent({ id: nextIntent.current++, ...next })
    window.location.hash = hrefFor('plan')
  }

  const mapPage = MAP_PAGES.includes(page)

  return (
    <div className="flex h-dvh flex-col bg-canvas text-ink">
      <a
        href="#main"
        onClick={(e) => {
          e.preventDefault()
          main.current?.focus()
        }}
        className="sr-only z-[2000] rounded-lg bg-surface px-3 py-2 text-sm font-medium shadow-pop focus:not-sr-only focus:absolute focus:top-2 focus:left-2"
      >
        Skip to content
      </a>

      <header className="pt-safe px-safe relative z-[1500] shrink-0 border-b border-line bg-surface/85 backdrop-blur-xl">
        <div className="mx-auto flex h-[var(--bar-h)] max-w-[100rem] items-center gap-6 px-4 lg:px-6">
          <a href="#/" className="flex items-center gap-2.5 rounded-lg focus-visible:outline-2 focus-visible:outline-offset-4 focus-visible:outline-brand">
            <Logo />
            <span className="text-lg font-semibold tracking-tight">Safar</span>
          </a>
          <nav aria-label="Main" className="hidden md:block">
            <ul className="flex gap-1">
              {[...TABS, { page: 'about' as const, label: 'About' }].map((item) => (
                <li key={item.page}>
                  <a
                    href={hrefFor(item.page)}
                    aria-current={page === item.page ? 'page' : undefined}
                    className={`flex h-9 items-center rounded-full px-3.5 text-sm font-medium transition-colors focus-visible:outline-2 focus-visible:outline-brand ${
                      page === item.page ? 'bg-brand-soft text-brand-ink' : 'text-muted hover:bg-subtle hover:text-ink'
                    }`}
                  >
                    {item.label}
                  </a>
                </li>
              ))}
            </ul>
          </nav>
          <div className="ml-auto flex items-center gap-1">
            {install && (
              <button
                type="button"
                onClick={() => void install()}
                className="inline-flex h-9 items-center gap-1.5 rounded-full border border-line px-3.5 text-sm font-semibold text-ink-2 transition-colors hover:bg-subtle focus-visible:outline-2 focus-visible:outline-brand pointer-coarse:h-11"
              >
                <Download className="size-4" aria-hidden="true" />
                <span className="hidden sm:inline">Install app</span>
                <span className="sm:hidden">Install</span>
              </button>
            )}
            <a
              href="#/about"
              aria-label="About Safar"
              aria-current={page === 'about' ? 'page' : undefined}
              className={`grid size-11 place-items-center rounded-full transition-colors focus-visible:outline-2 focus-visible:outline-brand md:hidden ${
                page === 'about' ? 'bg-brand-soft text-brand-ink' : 'text-muted hover:bg-subtle'
              }`}
            >
              <Info className="size-5" aria-hidden="true" />
            </a>
          </div>
        </div>
      </header>

      <ServerWaking />

      <main
        id="main"
        ref={main}
        tabIndex={-1}
        className={`relative min-h-0 flex-1 focus:outline-none ${mapPage ? 'overflow-clip' : 'overflow-y-auto overscroll-contain'}`}
      >
        {page === 'home' && (
          <div className="animate-fade">
            <HomePage
              network={network}
              install={install}
              onPlan={(from, to) => startPlan({ from, to })}
              onExample={(example) => startPlan({ example })}
            />
          </div>
        )}
        {visited.has('plan') && (
          <div hidden={page !== 'plan'} className="h-full animate-fade">
            <PlanPage network={network} closures={closures} intent={intent} />
          </div>
        )}
        {visited.has('lab') && (
          <div hidden={page !== 'lab'} className="h-full animate-fade">
            <Loading>
              <LabPage network={network} closures={closures} />
            </Loading>
          </div>
        )}
        {visited.has('trips') && (
          <div hidden={page !== 'trips'} className="h-full animate-fade">
            <Loading>
              <TripPage network={network} closures={closures} />
            </Loading>
          </div>
        )}
        {page === 'about' && (
          <div className="animate-fade">
            <Loading>
              <AboutPage />
            </Loading>
          </div>
        )}
      </main>

      <nav aria-label="Main" className="pb-safe px-safe relative z-[1500] shrink-0 border-t border-line bg-surface/90 backdrop-blur-xl md:hidden">
        <ul className="grid h-[var(--tab-h)] grid-cols-4">
          {TABS.map(({ page: p, label, short, icon: Icon }) => {
            const active = page === p
            return (
              <li key={p}>
                <a
                  href={hrefFor(p)}
                  aria-label={label}
                  aria-current={active ? 'page' : undefined}
                  className="group flex h-full flex-col items-center justify-center gap-1 text-[11px] font-medium focus-visible:outline-none"
                >
                  <span
                    className={`grid h-8 w-14 place-items-center rounded-full transition-colors group-focus-visible:outline-2 group-focus-visible:outline-brand ${
                      active ? 'bg-brand-soft text-brand-ink' : 'text-muted'
                    }`}
                  >
                    <Icon className="size-5" strokeWidth={active ? 2.25 : 1.75} aria-hidden="true" />
                  </span>
                  <span className={active ? 'font-semibold text-ink' : 'text-muted'}>{short ?? label}</span>
                </a>
              </li>
            )
          })}
        </ul>
      </nav>
    </div>
  )
}
