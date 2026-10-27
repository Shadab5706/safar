import { useMetroNetwork } from './hooks/useMetroNetwork'
import { useHashRoute, type Page } from './hooks/useHashRoute'
import { AboutPage } from './pages/AboutPage'
import { PlanPage } from './pages/PlanPage'

const NAV: { page: Page; href: string; label: string }[] = [
  { page: 'plan', href: '#/', label: 'Plan' },
  { page: 'about', href: '#/about', label: 'About' },
]

export default function App() {
  const page = useHashRoute()
  const network = useMetroNetwork()

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
          <span className="hidden text-sm text-teal-100 sm:inline">Delhi-NCR journey planner</span>
        </a>
        <nav aria-label="Main">
          <ul className="flex gap-1">
            {NAV.map((item) => (
              <li key={item.page}>
                <a
                  href={item.href}
                  aria-current={page === item.page ? 'page' : undefined}
                  className={`rounded-md px-3 py-1.5 text-sm font-medium focus-visible:outline-2 focus-visible:outline-white ${
                    page === item.page ? 'bg-white/15 text-white' : 'text-teal-50 hover:bg-white/10'
                  }`}
                >
                  {item.label}
                </a>
              </li>
            ))}
          </ul>
        </nav>
      </header>
      <main id="main" tabIndex={-1} className="flex-1 focus:outline-none lg:min-h-0">
        <div hidden={page !== 'plan'} className="lg:h-full">
          <PlanPage network={network} />
        </div>
        {page === 'about' && <AboutPage />}
      </main>
    </div>
  )
}
