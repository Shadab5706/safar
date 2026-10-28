# Safar

AI multi-modal journey planner for Delhi-NCR. It combines metro, cab, auto, bike taxi and walking
into one trip, and shows the time, distance and price of every route. The routing is done with
classic AI search (BFS, DFS, UCS, Greedy, A*) and local search (hill climbing, simulated annealing).

See [PLAN.md](PLAN.md) for the full design and milestones.

## Run the backend

Needs CMake ≥ 3.20, Ninja and a C++20 compiler. Dependencies are downloaded automatically.

```bash
cmake -S backend -B backend/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build backend/build
ctest --test-dir backend/build --output-on-failure
cd backend && ./build/safar_server config/server.json
```

Then open http://localhost:8080/api/v1/health

## Run the frontend

Needs Node.js ≥ 22.12. With the backend running on :8080 (the dev server proxies `/api` to it;
set `SAFAR_API` to point elsewhere):

```bash
cd frontend
npm install
npm run dev        # http://localhost:5173
```

`npm run lint`, `npm run typecheck`, `npm test` (Vitest) and `npm run build` are what CI runs.
React + TypeScript (strict) + Vite + Tailwind, with Leaflet and OpenStreetMap tiles (no API key).

The **Plan** screen has from/to autocomplete over places and metro stations, a departure time, mode
toggles (walk, metro, cab, auto, bike) and a cheapest↔fastest slider. Each route card shows its
tags, total time, arrival, fare, distance and changes; expand it for the timed step-by-step legs,
every provider quote on cab/auto/bike legs with an "open in app" link, and the metro tickets per
operator (official vs approximate fare). The selected route is drawn on the map leg by leg (metro in
line colours, road legs dashed, walks dotted) over a faint layer of every metro line. After the
first search, changing the modes, time or slider re-plans straight away. **About** lists the data
sources, OSM attribution and which prices are estimates.

## Rebuild the data

The processed data in `data/processed/` is committed, so this is only needed to refresh it.

```bash
python3 scripts/fetch_metro_osm.py      # download metro routes from OpenStreetMap
python3 scripts/build_metro_graph.py    # -> data/processed/metro.json
python3 scripts/build_places.py         # geocode data/curated/places.csv -> places.json
python3 scripts/prefetch_road_matrix.py # real road km + drive times from OSRM -> road_matrix.json
```

## Calibrate cab / auto / bike fares

Uber, Ola and Rapido have no public price API, so their fares are estimates from rate cards in
`backend/config/providers.json`. To make them data-driven, check the apps on real trips and add one
row per quote to `data/fare_samples.csv`:

```
provider,product,from,to,km,minutes,hour,quoted_fare,date,source
uber,go,Bennett University,Pari Chowk metro,7.4,16,9,178,2026-10-20,app
```

`km` / `minutes` are what the app shows for the trip, `hour` is 0-23, `source` is `app` (rows marked
`example` are illustrative and ignored). Then fit `fare / surge(hour) = base + per_km·km + per_min·min`
per product with least squares:

```bash
python3 scripts/fit_fares.py --dry-run   # print the MAE table only
python3 scripts/fit_fares.py             # also write the fitted values into providers.json
```

A product needs at least 3 quotes (8+ recommended). Fitted cards get `"basis": "fitted"` and a `fit`
block with the sample count, MAE and date. The Delhi auto meter is the government rate card and is
never refitted.

## API so far

| Endpoint | What it returns |
|---|---|
| `GET /api/v1/health` | status, version, loaded data counts |
| `GET /api/v1/places?q=hauz&limit=5` | autocomplete over places + metro stations |
| `GET /api/v1/metro/lines` | every line with colour, operator and station order |
| `GET /api/v1/metro/stations[/<id>]` | station coordinates and the lines serving them |
| `POST /api/v1/journeys` | multi-modal routes between two places/stations: walk + metro + cab/auto/bike, with time, fare, distance, tickets and tags (recommended, fastest, cheapest, fewest changes, most expensive). Every road leg lists per-provider quotes in `options` |
| `GET /api/v1/fares/estimate?from=&to=&at=` | every provider's quote for one road trip (Uber Go / Premier / XL, Ola Mini / Prime, Rapido Bike / Auto / Cab, Delhi auto meter), cheapest first |
| `POST /api/v1/lab/compare` | run BFS / DFS / UCS / Greedy / A* on one station-to-station query: route, cost, optimality gap, nodes expanded, runtime, expansion order |
| `GET /api/v1/disruptions` | current line closures (`items`, `version`) |
| `POST /api/v1/disruptions` | close a station `{"type":"station","station":"rajiv-chowk"}` or a section `{"type":"segment","line":"yellow","from":"green-park","to":"hauz-khas"}` (201; 200 with `"added": false` if already closed) |
| `DELETE /api/v1/disruptions[/<id>]` | reopen one closure (`station:<id>`, `segment:<line>:<a>:<b>`) or all of them |
| `POST /api/v1/trips/optimize` | best order to visit 3-10 stops (first = start, optional `round_trip`): hill climbing and/or simulated annealing on a matrix of planned journeys, with totals, each leg's route, convergence curves and the gap to the exact optimum |

Closures are server state (in memory, at most 50): journeys, the Algorithm Lab and the trip planner
all route around them. A closed station is skipped (no boarding, alighting or changing; trains
run through), a closed section stops trains between two neighbouring stations.

## Status

Phase 6 (Algorithm Lab, line closures, trip planner) done: 100 backend tests + 48 frontend tests.
Screens: Plan (`#/`), Algorithm Lab (`#/lab`), Trip planner (`#/trips`), About (`#/about`). Click a
station or a line on any map to close or reopen it. Next: Phase 7 (polish, Docker, report). See
PLAN.md §11.

Try it:

```bash
curl -s -X POST localhost:8080/api/v1/journeys \
  -d '{"from":"bennett-university","to":"hauz-khas-village","depart_at":"2026-10-27T09:00"}'
```

```bash
curl -s -X POST localhost:8080/api/v1/disruptions \
  -d '{"type":"segment","line":"yellow","from":"green-park","to":"hauz-khas"}'
```

```bash
curl -s -X POST localhost:8080/api/v1/trips/optimize \
  -d '{"stops":["connaught-place","india-gate","red-fort","qutub-minar","lotus-temple"]}'
```

```bash
curl -s 'localhost:8080/api/v1/fares/estimate?from=bennett-university&to=station:pari-chowk&at=2026-10-27T18:30'
```

Fares: DMRC slabs are the official ones from 25 Aug 2025. Airport Express, NMRC, Rapid Metro and
Namo Bharat fares are approximate (`"verified": false` in `backend/config/transport.json`).

Cab, auto and bike quotes are **estimates** (`"source": "estimate"` on every quote) from the rate
cards in `backend/config/providers.json`: hand-set approximations (`"basis": "approximate"`) until
refitted from real quotes (`"fitted"`), plus the Delhi govt auto meter card (`"govt_rate_card"`,
2022 notification, not re-checked against the gazette). The search prices each road edge with the
cheapest product of that vehicle type; the route then lists every product. Each quote has a
`deep_link` to see the live price: Uber's documented universal link (pickup and drop filled in),
Ola's web/app link (filled in, but the format is undocumented: `"documented": false`), and the
Rapido site (no pre-fill format exists, `"prefilled": false`). Links carry only public coordinates
and place names.

Map and road data © OpenStreetMap contributors (ODbL). Road times via OSRM.
