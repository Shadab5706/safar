import { useEffect, useState, type ReactNode } from 'react'
import { getHealth } from '../api/client'
import type { Health } from '../api/types'

function useHealth(): Health | null {
  const [health, setHealth] = useState<Health | null>(null)
  useEffect(() => {
    const ctl = new AbortController()
    getHealth(ctl.signal).then(setHealth, () => undefined)
    return () => ctl.abort()
  }, [])
  return health
}

function Section({ title, children }: { title: string; children: ReactNode }) {
  return (
    <section className="rounded-xl border border-slate-200 bg-white p-5">
      <h2 className="mb-2 text-base font-bold text-slate-900">{title}</h2>
      <div className="space-y-2 text-sm leading-relaxed text-slate-700">{children}</div>
    </section>
  )
}

const ext = { target: '_blank', rel: 'noopener noreferrer', className: 'font-medium text-teal-800 underline underline-offset-2' }

export function AboutPage() {
  const health = useHealth()
  return (
    <div className="mx-auto max-w-3xl space-y-4 p-4 sm:p-6">
      <header>
        <h1 className="text-2xl font-bold text-slate-900">About Safar</h1>
        <p className="mt-1 text-slate-600">
          A journey planner for Delhi-NCR that combines metro, cab, auto, bike taxi and walking into one trip, and shows
          the time, distance and price of every route.
        </p>
      </header>

      <Section title="How routes are found">
        <p>
          The metro network and the city&apos;s places form one graph: stations, platforms and places are nodes; rides,
          changes, walks and road trips are edges that carry both minutes and rupees. Routes are searched with A*,
          guided by straight-line distance, and Yen&apos;s algorithm finds several different good paths.
        </p>
        <p>
          The planner trades time against money by sweeping a value of time from ₹0.5 to ₹30 per minute, keeps only
          routes that no other route beats on both, and tags the recommended, fastest, cheapest, fewest-changes and most
          expensive options. The preference slider moves the recommendation between cheapest and fastest.
        </p>
      </Section>

      <Section title="Which prices are estimates">
        <ul className="list-disc space-y-1.5 pl-5">
          <li>
            <strong>Delhi Metro (DMRC):</strong> the official fare slabs from 25 Aug 2025, including Sunday fares. Shown
            as <span className="font-medium text-emerald-700">Official fare</span>.
          </li>
          <li>
            <strong>Airport Express, Noida Metro, Rapid Metro Gurugram and Namo Bharat:</strong> approximate fares,
            shown as <span className="font-medium text-amber-800">Approx. fare</span>. One ticket is bought per
            operator.
          </li>
          <li>
            <strong>Uber, Ola and Rapido:</strong> estimates from rate cards, because these services have no public
            price API. Every such price is marked <span className="font-medium text-amber-800">est.</span> with a
            likely range, and an <em>Open in app</em> link shows the live fare. Uber links fill in pickup and drop;
            Ola&apos;s link format is unofficial; Rapido opens its website.
          </li>
          <li>
            <strong>Auto (meter):</strong> the Delhi government auto-rickshaw rate card (2022), with the night charge
            from 11 pm to 5 am. Many autos quote a fixed price instead.
          </li>
          <li>
            <strong>Times:</strong> road times are from OSRM with an hour-of-day traffic factor; metro times use
            average waits and station-to-station run times. Real trips can differ.
          </li>
        </ul>
      </Section>

      <Section title="Data sources">
        <ul className="list-disc space-y-1.5 pl-5">
          <li>
            Metro lines, stations and map tiles: ©{' '}
            <a href="https://www.openstreetmap.org/copyright" {...ext}>
              OpenStreetMap contributors
            </a>
            , available under the Open Database License (ODbL).
          </li>
          <li>
            Place coordinates: geocoded with{' '}
            <a href="https://nominatim.org/" {...ext}>
              Nominatim
            </a>{' '}
            (OpenStreetMap data).
          </li>
          <li>
            Road distances and drive times:{' '}
            <a href="https://project-osrm.org/" {...ext}>
              OSRM
            </a>{' '}
            (Open Source Routing Machine), on OpenStreetMap roads.
          </li>
          <li>Metro fares: published DMRC fare table; operator fare charts for the other networks (approximate).</li>
          <li>Cab, auto and bike rate cards: approximate published rates, to be refitted from real app quotes.</li>
        </ul>
        {health && (
          <p className="pt-1 text-xs text-slate-500">
            Loaded now: {health.data.lines} metro lines, {health.data.stations} stations, {health.data.places} places,{' '}
            {health.data.road_pairs.toLocaleString('en-IN')} road trips · server v{health.version}
          </p>
        )}
      </Section>

      <p className="pb-4 text-center text-xs text-slate-500">
        Safar is a student project for the Application of AI course (CSET381) at Bennett University. Plans are a guide:
        check the operator&apos;s app before you travel.
      </p>
    </div>
  )
}
