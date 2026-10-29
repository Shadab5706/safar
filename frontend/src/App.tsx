import { useEffect, useState } from 'react'
import { getHealth, wakeApi } from './api/client'
import { ServerWaking } from './components/ServerWaking'
import { useClosures } from './hooks/useClosures'
import { useMetroNetwork } from './hooks/useMetroNetwork'
import { useHashRoute, type Page } from './hooks/useHashRoute'
import { AboutPage } from './pages/AboutPage'
import { LabPage } from './pages/LabPage'
import { PlanPage } from './pages/PlanPage'
import { TripPage } from './pages/TripPage'

const NAV: { page: Page; href: string; label: string; short?: string }[] = [
  { page: 'plan', href: '#/', label: 'Plan' },
  { page: 'lab', href: '#/lab', label: 'Algorithm Lab', short: 'Lab' },
  { page: 'trips', href: '#/trips', label: 'Trip planner', short: 'Trips' },
  { page: 'about', href: '#/about', label: 'About' },
]

export default function App() {
  const page = useHashRoute()
  const network = useMetroNetwork()
  const closures = useClosures()
  // Screens mount on first visit and then stay mounted, so switching tabs keeps their state.
  const [visited, setVisited] = useState<ReadonlySet<Page>>(() => new Set([page]))
  if (!visited.has(page)) setVisited(new Set([...visited, page]))

  // Start a sleeping server as soon as the page opens, so it is up by the time a search is sent.
  useEffect(() => {
    void wakeApi()
    getHealth().catch(() => {})
  }, [])

  return (
    <div className="flex min-h-dvh flex-col bg-slate-50 text-slate-900 lg:h-dvh">
      <a
        href="#main"
        onClick={(e) => {
          e.preventDefault()
          document.getElementById('main')?.focus()
        }}
        className="sr-only z-[2000] rounded bg-white px-3 py-2 text-sm font-medium focus:not-sr-only focus:absolute focus:top-2 focus:left-2"
      >
        Skip to content
      </a>
      <header className="flex items-center justify-between gap-4 border-b border-teal-900/20 bg-teal-800 px-4 py-2.5 text-white">
        <a href="#/" className="flex items-center gap-2 rounded focus-visible:outline-2 focus-visible:outline-white">
          <img src="/favicon.svg" alt="" className="size-7" />
          <span className="text-lg font-bold tracking-tight">Safar</span>
          <span className="hidden text-sm text-teal-100 xl:inline">Delhi-NCR journey planner</span>
        </a>
        <nav aria-label="Main">
          <ul className="flex gap-0.5 sm:gap-1">
            {NAV.map((item) => (
              <li key={item.page}>
                <a
                  href={item.href}
                  aria-label={item.label}
                  aria-current={page === item.page ? 'page' : undefined}
                  className={`block rounded-md px-2 py-1.5 text-sm font-medium sm:px-3 focus-visible:outline-2 focus-visible:outline-white ${
                    page === item.page ? 'bg-white/15 text-white' : 'text-teal-50 hover:bg-white/10'
                  }`}
                >
                  {item.short ? (
                    <>
                      <span className="sm:hidden">{item.short}</span>
                      <span className="hidden sm:inline">{item.label}</span>
                    </>
                  ) : (
                    item.label
                  )}
                </a>
              </li>
            ))}
          </ul>
        </nav>
      </header>
      <ServerWaking />
      <main id="main" tabIndex={-1} className="flex-1 focus:outline-none lg:min-h-0">
        {visited.has('plan') && (
          <div hidden={page !== 'plan'} className="lg:h-full">
            <PlanPage network={network} closures={closures} />
          </div>
        )}
        {visited.has('lab') && (
          <div hidden={page !== 'lab'} className="lg:h-full">
            <LabPage network={network} closures={closures} />
          </div>
        )}
        {visited.has('trips') && (
          <div hidden={page !== 'trips'} className="lg:h-full">
            <TripPage network={network} closures={closures} />
          </div>
        )}
        {page === 'about' && <AboutPage />}
      </main>
    </div>
  )
}
