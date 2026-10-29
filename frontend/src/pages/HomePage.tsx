import {
  ArrowRight,
  BadgeIndianRupee,
  Construction,
  Download,
  FlaskConical,
  Navigation,
  Route as RouteIcon,
  Sparkles,
  Waypoints,
  type LucideIcon,
} from 'lucide-react'
import { useState, type FormEvent, type ReactNode } from 'react'
import type { SearchResult } from '../api/types'
import { EndpointFields } from '../components/JourneyForm'
import { ModeIcon } from '../components/ModeIcon'
import type { NetworkState } from '../hooks/useMetroNetwork'
import { EXAMPLE_TRIPS, type ExampleTrip } from '../lib/examples'
import { modeLabel } from '../lib/format'
import { ALL_MODES } from '../lib/query'

const NO_LINES = new Map()

interface Props {
  network: NetworkState
  onPlan: (from: SearchResult | null, to: SearchResult | null) => void
  onExample: (trip: ExampleTrip) => void
  install: (() => Promise<void>) | null
}

/** Stylised metro lines behind the hero. Decorative only. */
function MetroArt() {
  const lines = [
    { d: 'M-20 250 C 140 240, 220 120, 380 120 S 600 40, 720 30', c: '#FFCC00' },
    { d: 'M-20 60 C 160 80, 250 210, 400 220 S 610 320, 720 300', c: '#4169E1' },
    { d: 'M60 340 C 160 260, 300 300, 380 200 S 520 40, 640 -20', c: '#CC338B' },
    { d: 'M-20 170 C 200 170, 300 160, 420 170 S 620 190, 720 160', c: '#00B5E2' },
  ]
  const stops: [number, number][] = [
    [380, 120],
    [400, 220],
    [380, 200],
    [420, 170],
    [212, 160],
    [560, 178],
  ]
  return (
    <svg viewBox="0 0 700 340" preserveAspectRatio="xMidYMid slice" className="absolute inset-0 size-full" aria-hidden="true">
      {lines.map((l) => (
        <path key={l.c} d={l.d} fill="none" stroke={l.c} strokeWidth={7} strokeLinecap="round" opacity={0.55} />
      ))}
      {stops.map(([x, y]) => (
        <circle key={`${x}:${y}`} cx={x} cy={y} r={7} fill="var(--surface)" stroke="var(--ink)" strokeWidth={3} opacity={0.75} />
      ))}
    </svg>
  )
}

function Feature({ icon: Icon, title, children, extra }: { icon: LucideIcon; title: string; children: ReactNode; extra?: ReactNode }) {
  return (
    <li className="rounded-card border border-line bg-surface p-5 shadow-card">
      <span className="grid size-10 place-items-center rounded-xl bg-brand-soft text-brand-ink">
        <Icon className="size-5" aria-hidden="true" />
      </span>
      <h3 className="mt-4 text-base font-semibold tracking-tight text-ink">{title}</h3>
      <p className="mt-1.5 text-sm leading-relaxed text-muted">{children}</p>
      {extra}
    </li>
  )
}

const ENTRIES = [
  {
    href: '#/plan',
    icon: Navigation,
    title: 'Plan a journey',
    text: 'Routes that mix the metro with cabs, autos, bike taxis and walking, each with its time and price.',
  },
  {
    href: '#/lab',
    icon: FlaskConical,
    title: 'Algorithm Lab',
    text: 'Watch BFS, DFS, UCS, Greedy and A* explore the metro network, and compare what each one finds.',
  },
  {
    href: '#/trips',
    icon: RouteIcon,
    title: 'Trip planner',
    text: 'Visiting several places in a day? Hill climbing and simulated annealing find a good order.',
  },
]

export function HomePage({ network, onPlan, onExample, install }: Props) {
  const [from, setFrom] = useState<SearchResult | null>(null)
  const [to, setTo] = useState<SearchResult | null>(null)
  const lineById = network.status === 'ready' ? network.network.lineById : NO_LINES
  const stats = network.status === 'ready' ? network.network : null

  function submit(e: FormEvent) {
    e.preventDefault()
    if (from || to) onPlan(from, to)
  }

  return (
    <div className="pb-10">
      <section aria-labelledby="hero-title" className="relative overflow-hidden border-b border-line" style={{ backgroundImage: 'var(--hero-glow)' }}>
        <div className="pointer-events-none absolute inset-y-0 right-0 hidden w-[58%] [mask-image:linear-gradient(to_left,black_30%,transparent)] lg:block">
          <MetroArt />
        </div>
        <div className="relative mx-auto grid max-w-6xl items-center gap-8 px-4 pt-8 pb-10 sm:px-6 sm:pt-12 lg:grid-cols-[1.05fr_1fr] lg:gap-14 lg:pt-20 lg:pb-24">
          <div className="animate-rise">
            <p className="inline-flex items-center gap-2 rounded-full border border-line bg-surface/70 px-3 py-1 text-xs font-medium text-muted backdrop-blur">
              <span className="size-1.5 rounded-full bg-accent" aria-hidden="true" />
              Delhi-NCR journey planner
            </p>
            <h1 id="hero-title" className="mt-4 text-[2.15rem] leading-[1.08] font-semibold tracking-tight text-balance text-ink sm:text-5xl lg:text-6xl">
              Every way across Delhi, <span className="text-brand-ink">in one search.</span>
            </h1>
            <p className="mt-4 max-w-xl text-base leading-relaxed text-pretty text-muted sm:text-lg">
              Safar mixes the metro with cabs, autos, bike taxis and walking, then shows the time, distance and price of
              every route worth taking.
            </p>
            {stats && (
              <dl className="mt-6 hidden gap-8 sm:flex">
                {[
                  [String(stats.stations.length), 'metro stations'],
                  [String(stats.lines.length), 'lines, incl. Rapid Metro and Namo Bharat'],
                  ['4', 'ways to ride, plus walking'],
                ].map(([n, label]) => (
                  <div key={label} className="max-w-36">
                    <dt className="sr-only">{label}</dt>
                    <dd>
                      <span className="block text-2xl font-semibold tracking-tight text-ink tabular-nums">{n}</span>
                      <span className="text-xs leading-snug text-muted">{label}</span>
                    </dd>
                  </div>
                ))}
              </dl>
            )}
          </div>

          <form
            onSubmit={submit}
            aria-label="Find routes"
            className="animate-rise rounded-[1.5rem] border border-line bg-surface p-4 shadow-pop [animation-delay:80ms] sm:p-6"
          >
            <h2 className="mb-3 text-sm font-semibold text-ink">Where are you going?</h2>
            <EndpointFields
              from={from}
              to={to}
              onChange={(patch) => {
                if ('from' in patch) setFrom(patch.from ?? null)
                if ('to' in patch) setTo(patch.to ?? null)
              }}
              lineById={lineById}
            />
            <button
              type="submit"
              disabled={!from && !to}
              className="mt-4 inline-flex h-12 w-full items-center justify-center gap-2 rounded-control bg-brand px-4 text-base font-semibold text-on-brand shadow-card transition hover:bg-brand-strong focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-brand active:scale-[0.99] disabled:cursor-not-allowed disabled:bg-subtle-2 disabled:text-muted disabled:shadow-none"
            >
              Find routes
              <ArrowRight className="size-4" aria-hidden="true" />
            </button>
            <div className="mt-4 flex flex-wrap items-center gap-1.5">
              {ALL_MODES.map((m) => (
                <span key={m} className="grid size-8 place-items-center rounded-full bg-subtle text-muted" title={modeLabel(m)}>
                  <ModeIcon mode={m} className="size-4" />
                </span>
              ))}
              <span className="ml-1 text-xs text-muted">Choose modes and time on the next screen.</span>
            </div>
          </form>
        </div>
      </section>

      <section aria-labelledby="examples-title" className="mx-auto max-w-6xl px-4 pt-10 sm:px-6">
        <h2 id="examples-title" className="text-xs font-semibold tracking-wide text-muted uppercase">
          Popular trips
        </h2>
        <ul className="mt-3 grid gap-3 md:grid-cols-3">
          {EXAMPLE_TRIPS.map((trip) => (
            <li key={trip.from.id + trip.to.id}>
              <button
                type="button"
                onClick={() => onExample(trip)}
                className="group flex h-full min-h-16 w-full items-center gap-3 rounded-card border border-line bg-surface p-4 text-left shadow-card transition hover:-translate-y-0.5 hover:border-brand/40 hover:shadow-lift focus-visible:outline-2 focus-visible:outline-brand"
              >
                <span className="flex flex-col items-center gap-1 self-stretch py-1" aria-hidden="true">
                  <span className="size-2.5 rounded-full border-2 border-brand" />
                  <span className="w-px flex-1 border-l-2 border-dotted border-line-strong" />
                  <span className="size-2.5 rounded-full bg-accent" />
                </span>
                <span className="min-w-0 flex-1">
                  <span className="block truncate text-sm font-semibold text-ink">{trip.from.name}</span>
                  <span className="block truncate text-sm font-semibold text-ink">{trip.to.name}</span>
                  <span className="mt-1 block text-xs text-muted">{trip.note}</span>
                </span>
                <ArrowRight className="size-4 shrink-0 text-faint transition-transform group-hover:translate-x-0.5 group-hover:text-brand-ink" aria-hidden="true" />
              </button>
            </li>
          ))}
        </ul>
      </section>

      <section aria-labelledby="how-title" className="mx-auto max-w-6xl px-4 pt-14 sm:px-6">
        <h2 id="how-title" className="text-2xl font-semibold tracking-tight text-ink sm:text-3xl">
          How it works
        </h2>
        <p className="mt-2 max-w-2xl text-muted">One search looks at every combination, so you don&apos;t have to check four apps.</p>
        <ul className="mt-6 grid gap-4 sm:grid-cols-2 lg:grid-cols-4">
          <Feature
            icon={Waypoints}
            title="Every mode, combined"
            extra={
              <div className="mt-3 flex gap-1.5" aria-hidden="true">
                {(['metro', 'cab', 'auto', 'bike', 'walk'] as const).map((m) => (
                  <span key={m} className="grid size-7 place-items-center rounded-lg bg-subtle text-ink-2">
                    <ModeIcon mode={m} className="size-3.5" />
                  </span>
                ))}
              </div>
            }
          >
            Take an auto to the metro, ride two lines, then a bike taxi for the last stretch, all in one plan.
          </Feature>
          <Feature icon={BadgeIndianRupee} title="Prices side by side">
            Uber, Ola and Rapido estimates and the auto meter next to exact DMRC fares, with buttons that open the app with
            your trip filled in.
          </Feature>
          <Feature icon={Sparkles} title="AI search, in milliseconds">
            A* search, k-shortest paths and a time-versus-money sweep pick the routes worth showing: fastest, cheapest
            and the best trade-offs.
          </Feature>
          <Feature icon={Construction} title="Ready for disruptions">
            Close a station or a section of a line and every route re-plans around it, the way a real network has to.
          </Feature>
        </ul>
      </section>

      <section aria-labelledby="explore-title" className="mx-auto max-w-6xl px-4 pt-14 sm:px-6">
        <h2 id="explore-title" className="text-2xl font-semibold tracking-tight text-ink sm:text-3xl">
          Explore Safar
        </h2>
        <ul className="mt-6 grid gap-4 md:grid-cols-3">
          {ENTRIES.map(({ href, icon: Icon, title, text }) => (
            <li key={href}>
              <a
                href={href}
                className="group relative flex h-full flex-col overflow-hidden rounded-card border border-line bg-surface p-5 shadow-card transition hover:-translate-y-0.5 hover:border-brand/40 hover:shadow-lift focus-visible:outline-2 focus-visible:outline-brand"
              >
                <span className="grid size-11 place-items-center rounded-xl bg-brand text-on-brand shadow-card">
                  <Icon className="size-5" aria-hidden="true" />
                </span>
                <span className="mt-4 text-lg font-semibold tracking-tight text-ink">{title}</span>
                <span className="mt-1.5 flex-1 text-sm leading-relaxed text-muted">{text}</span>
                <span className="mt-4 inline-flex items-center gap-1 text-sm font-semibold text-brand-ink">
                  Open
                  <ArrowRight className="size-4 transition-transform group-hover:translate-x-0.5" aria-hidden="true" />
                </span>
              </a>
            </li>
          ))}
        </ul>
      </section>

      <footer className="mx-auto mt-14 flex max-w-6xl flex-col gap-4 border-t border-line px-4 pt-6 text-xs text-muted sm:flex-row sm:items-center sm:px-6">
        <p className="flex-1 leading-relaxed">
          Cab, auto and bike taxi prices are estimates; open the provider&apos;s app for the live fare. Map data ©{' '}
          <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noopener noreferrer" className="underline underline-offset-2">
            OpenStreetMap
          </a>{' '}
          contributors.
        </p>
        <div className="flex items-center gap-2">
          {install && (
            <button
              type="button"
              onClick={() => void install()}
              className="inline-flex h-10 items-center gap-1.5 rounded-full bg-ink px-4 text-sm font-semibold text-on-brand transition hover:opacity-90 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-brand"
            >
              <Download className="size-4" aria-hidden="true" />
              Install the app
            </button>
          )}
          <a href="#/about" className="inline-flex h-10 items-center rounded-full border border-line px-4 text-sm font-semibold text-ink-2 hover:bg-subtle">
            About Safar
          </a>
        </div>
      </footer>
    </div>
  )
}
