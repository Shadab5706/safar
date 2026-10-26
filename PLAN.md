# Safar — AI Multi-Modal Journey Planner for Delhi-NCR

Course: Application of AI (B.Tech CSE, 3rd year)
Status: Phase 2 (search core) done — 2026-10-26. Next: Phase 3 (multi-modal journeys)

---

## 1. The pitch

You type "Bennett University → Hauz Khas Village". Safar gives you every sensible way to get
there, mixing transport modes in one trip:

> Rapido auto to Pari Chowk (12 min, ₹58) → Aqua Line to Sector 51 → walk 4 min to Sector 52 →
> Blue Line to Rajiv Chowk → Yellow Line to Hauz Khas → walk 8 min
> **Total: 1 h 52 min · ₹121 · 3 changes · 47.3 km**

Each route shows **price, distance and total time**, with tags like *Fastest*, *Cheapest*,
*Fewest changes*, *Most expensive* and *Recommended*. Cab legs show a per-provider breakdown
(Uber / Ola / Rapido / auto meter), and the routes can be re-ranked with a time-vs-money slider.

**What makes it different from Google Maps / the DMRC app**
1. It plans *combined* trips (cab for the last few km + metro for the long stretch). Existing
   apps mostly show transit *or* cab, rarely an optimised mix.
2. It compares prices across cab providers in one place.
3. It shows how the AI finds the route: watch each search algorithm run live and compare their
   numbers (nodes explored, time, path cost, whether it found the best route).
4. It handles disruptions ("Blue Line closed between Rajiv Chowk and Mandi House") and re-plans
   live, and it orders multi-stop day trips (TSP).

---

## 2. How it maps to the syllabus

| Syllabus topic (Unit 1) | Where it lives in Safar |
|---|---|
| Problem solving as search, state space | Journey = search over (location, current mode) states |
| Uninformed search: BFS, DFS, UCS | Algorithm Lab compare mode; BFS = fewest stops, UCS = baseline optimal |
| Informed search: Greedy best-first, A* | A* is the production router; haversine/top-speed heuristic (admissible, proven in report) |
| Heuristics: admissibility, consistency | Report section + a test that checks h(n) ≤ true cost |
| Local search: hill climbing, simulated annealing | Trip planner: best order for 5–10 stops (TSP) |
| Local optima, annealing schedule | Convergence chart in UI; hill climbing gets stuck, SA escapes it |
| Adversarial search (minimax, α-β) | Not a natural fit; stretch idea in §11 if the teacher wants it |
| Neural networks / learning | Fare model fitted on real collected fare quotes (regression → small MLP stretch) |

---

## 3. Feature list

**MVP (must have for the presentation)**
- Place search with autocomplete (curated ~150 Delhi-NCR places + all metro stations)
- Multi-modal journeys: walk + metro + cab + auto + bike taxi
- Top 3–5 alternative routes, each with legs, time, cost and distance
- Tags: fastest / cheapest / fewest changes / most expensive / recommended
- Time-vs-money preference slider (re-ranks routes)
- Cab price breakdown per provider for every road leg
- Map view: route drawn per leg, coloured by mode/metro line
- Algorithm Lab: run BFS / DFS / UCS / Greedy / A* on the same query, animate the explored
  nodes, show a stats table

**v2 (makes it stand out)**
- Disruptions: close stations/segments, see re-routing live
- Trip planner: pick stops, optimise the order with hill climbing vs simulated annealing
- Time-of-day: rush-hour cab speeds and surge multipliers
- "Open in Uber / Ola / Rapido" deep links with pickup/drop filled in
- Run history saved to a database, giving the data for the report's analysis section

**Stretch**
- DTC buses (from the Delhi Open Transit Data GTFS feed)
- Live fare APIs (if partner access is ever granted, see §6)
- PWA / installable mobile view

---

## 4. Architecture

```
┌──────────────────────────┐        HTTPS/JSON        ┌───────────────────────────────────┐
│  Frontend (React + TS)   │ ───────────────────────▶ │  Backend (C++20, cpp-httplib)     │
│  Vite · Tailwind ·       │                          │                                   │
│  Leaflet map             │ ◀─────────────────────── │  api/       REST handlers         │
└──────────────────────────┘                          │  search/    BFS DFS UCS Greedy A* │
                                                      │             + k-alternative routes│
                                                      │  optimize/  hill climbing, SA     │
                                                      │  graph/     multi-modal graph     │
                                                      │  fares/     fare engine+providers │
                                                      │  roads/     road time/distance    │
                                                      │  store/     SQLite cache + logs   │
                                                      └──────────────┬────────────────────┘
                                                                     │
                     ┌───────────────────────────────┬───────────────┴──────────────┐
                     ▼                               ▼                              ▼
          data/processed/*.json            OSRM (OpenStreetMap)          Fare provider adapters
          metro network + places           road distance & time          (estimate now, live later)
          (built offline by scripts/)      cached in SQLite
```

**Why these choices**
- **C++20 + cpp-httplib**: a single-header HTTP server, multithreaded, with no heavy framework to
  fight. All the AI logic is plain C++ the examiner can read.
- **nlohmann/json, spdlog, SQLite, GoogleTest**: industry standard, pulled in by CMake
  FetchContent, so nothing has to be installed by hand.
- **React + TypeScript + Vite + Tailwind + Leaflet**: a production-grade frontend standard.
  Leaflet with OpenStreetMap tiles is free and needs no API key.
- **OSRM** for road distances/times: free and open-source (OpenStreetMap data). A Google
  Routes API adapter can be plugged in later if you get a key (it needs a billing account).

---

## 5. Data

| Data | Source | How we get it |
|---|---|---|
| All metro lines (DMRC 12 lines, Aqua/NMRC, Rapid Metro, Namo Bharat RRTS): stations, order, coordinates | OpenStreetMap route relations via Overpass API (ODbL). DMRC GTFS (otd.delhi.gov.in) kept as a cross-check option | `scripts/fetch_metro_osm.py` → `scripts/build_metro_graph.py` → `data/processed/metro.json` |
| Metro travel time between stations | track distance ÷ calibrated running speed + dwell per mode, checked against published end-to-end times | baked into metro.json |
| Interchange walk times + extra walkways | hand-curated (e.g. Kashmere Gate 6 min, Dhaula Kuan skywalk) + auto-detected walk links ≤ 700 m | `data/curated/interchanges.json` |
| Metro fares | DMRC distance-based fare slabs (Sunday/holiday slab too) | `backend/config/fares.json` (verify current slabs before demo) |
| Places (POIs) | 145 curated places (universities, malls, monuments, hospitals, railway/bus stations, airports, business hubs) geocoded with OSM Nominatim | `data/curated/places.csv` → `scripts/build_places.py` → `data/processed/places.json` |
| Road distance/time between points | OSRM `table` + `route` API, cached | SQLite `road_cache` table |
| Cab/auto/bike fares | Fare engine (see §6) | `backend/config/fares.json` + `data/fare_samples.csv` |

Raw downloads go in `data/raw/` (git-ignored). Hand-written inputs live in `data/curated/`, generated JSON in `data/processed/`; both are committed.

---

## 6. Prices: the honest plan

**The reality, which you need to be able to explain in the viva:**
Uber, Ola and Rapido do **not** offer public price APIs for student or third-party apps in India.
Uber's price-estimate API is limited to approved business partners, Ola closed its public
developer API, and Rapido has never had one. Scraping their private app APIs breaks their terms
of service and stops working whenever they change something. So "live prices from their servers"
isn't possible legally for this project.

**What we build instead (and it's a stronger AI story):**
1. **`FareProvider` interface** in C++, one adapter per provider (Uber Go/Premier/XL, Ola
   Mini/Prime, Rapido Bike/Auto/Cab, Delhi auto meter, DMRC/NMRC metro). Each adapter returns a
   quote with `source = "estimate" | "live"`. If any provider ever grants API access, only that
   adapter changes.
2. **Data-driven fare model.** We collect 40–60 real quotes by checking the apps on real trips at
   different times (screenshots → `data/fare_samples.csv`). Then we fit
   `fare = base + a·km + b·min` per provider and vehicle with least-squares regression, plus a
   time-of-day surge factor. That's the machine-learning part of the project and goes in the report
   with an error table (MAE per provider).
3. **Official rate cards** where they exist (Delhi govt auto meter fare, DMRC slabs) are used
   directly.
4. **"Check live price" button** on each cab leg opens the Uber/Ola/Rapido app with pickup and drop
   already filled in, so the user sees the real live price in one tap.
5. The UI always labels estimates clearly ("≈ ₹240 · estimated").

---

## 7. The AI core

### 7.1 Graph model
- **Nodes:** places (POIs), metro *station-line* nodes (Rajiv Chowk has one node for Blue and one
  for Yellow, so line changes cost something), and the user's start/end points (added per query).
- **Edges, each with `mode`, `time_min`, `distance_km`, `fare` (or fare function):**
  - `METRO_RIDE`: consecutive stations on one line
  - `TRANSFER`: between line nodes of the same station (walk + average wait)
  - `WALK`: place ↔ station or place ↔ place if under ~1.2 km
  - `CAB` / `AUTO` / `BIKE`: place ↔ station / place ↔ place, from OSRM time + fare engine
  - `ENTRY/EXIT`: station gate time + platform wait

### 7.1a As built (Phase 2)
- Gate node per station (street level) + platform node per station-line (`rajiv-chowk@yellow`).
- Edges: Board (entry 4 min + line's average wait), Alight (exit 2.5 min), Ride, Transfer
  (walk + wait for the new line), Walk (gate to gate).
- Invariant checked at startup: every in-station transfer is quicker than exit + re-entry,
  so optimal routes never "leave and come back in".
- Heuristic speed = the fastest straight-line speed any edge achieves (about 84 km/h, set by Namo
  Bharat). That makes h consistent by the triangle inequality, which a test checks on every edge.

### 7.2 Search problem
- **State:** `(node, mode_used_to_arrive)`, so switching mode can carry a penalty
  (e.g. 3–5 min waiting for a cab).
- **Cost:** `g = w_t·time + w_c·money + w_x·transfers`, with weights set by the preference slider.
- **Heuristic (A\*):** `h = w_t · haversine(node, goal) / v_max`, where `v_max` is the fastest
  mode's top speed. It never overestimates, so A* returns the optimal route (proof in report).
- **Algorithms implemented:** BFS, DFS, UCS, Greedy best-first, A*, behind one interface:
  `SearchResult run(const Graph&, Query, SearchStats&)`.
- **Stats recorded:** nodes expanded, nodes generated, max frontier size, runtime (µs), path
  cost, and the expansion order (to animate in the UI).

### 7.3 Multiple good routes
1. Run A* with three weight presets (fastest, cheapest, fewest transfers).
2. Run **Yen's k-shortest paths** (k = 5) on the recommended weights for alternatives.
3. Merge results, drop near-duplicates (more than 70% of legs shared), keep the
   **Pareto-optimal** set on (time, cost, transfers), then tag the winners.

### 7.4 Trip planner (local search)
- State = an order of stops. Neighbour = 2-opt swap. Cost = total journey cost from the router.
- Hill climbing (with random restarts) vs simulated annealing (geometric cooling schedule).
- The UI plots the cost-per-iteration curve for both, so you can see SA escape local optima.

### 7.5 Disruptions
Closing a station or segment adds it to a blocked set the search skips; then it re-plans. This
shows the search adapting (and a cab bridging the gap).

---

## 8. REST API (v1)

| Method | Path | Purpose |
|---|---|---|
| GET  | `/api/v1/health` | status, version, data loaded counts |
| GET  | `/api/v1/places?q=` | autocomplete places + stations |
| GET  | `/api/v1/metro/lines` | lines with colours and station lists (for map) |
| POST | `/api/v1/journeys` | **main endpoint**: from, to, departAt, preference, allowed modes → ranked routes |
| POST | `/api/v1/lab/compare` | run chosen algorithms on one query → stats + expansion order |
| POST | `/api/v1/trips/optimize` | stops + method (hill_climb / sa) → best order + convergence curve |
| GET  | `/api/v1/fares/estimate?from=&to=` | per-provider cab/auto/bike quotes for one road leg |
| GET/POST/DELETE | `/api/v1/disruptions` | list / add / clear closed stations or segments |

Every response has the same shape: `{ "ok": true, "data": ..., "meta": { "took_ms": .. } }` or
`{ "ok": false, "error": { "code", "message" } }`. Request validation, CORS, request IDs and
structured logs are included.

---

## 9. Frontend screens
1. **Plan a journey:** from/to autocomplete, depart time, mode toggles, preference slider →
   route cards (tags, totals, expandable step-by-step legs, provider price table) + map.
2. **Algorithm Lab:** same query run with several algorithms; animated exploration on the map;
   stats table + bar charts.
3. **Trip Planner:** pick stops on the map → optimised order, total time/cost, convergence chart.
4. **Disruptions:** click a station/segment to close it; the current journey re-plans live.
5. **About:** how it works, data sources, which prices are estimates.

---

## 10. Repository layout

```
safar/
├── PLAN.md, README.md
├── backend/
│   ├── CMakeLists.txt
│   ├── config/            fares.json, server.json
│   ├── src/
│   │   ├── main.cpp
│   │   ├── api/           routes, request validation, JSON responses
│   │   ├── graph/         Graph, Node, Edge, builders
│   │   ├── search/        bfs, dfs, ucs, greedy, astar, yen_ksp, ranking
│   │   ├── optimize/      hill_climbing, simulated_annealing
│   │   ├── fares/         FareProvider interface + uber/ola/rapido/auto/metro adapters
│   │   ├── roads/         OSRM client + cache
│   │   ├── store/         SQLite wrapper
│   │   └── util/          geo (haversine), time, config, logging
│   └── tests/             GoogleTest unit + API tests
├── frontend/              React + TS + Vite + Tailwind + Leaflet
├── data/
│   ├── raw/               downloaded GTFS etc. (git-ignored)
│   ├── curated/           hand-written inputs: places.csv, interchanges.json
│   ├── processed/         generated: metro.json, places.json
│   └── fare_samples.csv   real fare quotes collected for calibration
├── scripts/               Python: build_metro_graph, fit_fares, prefetch_road_matrix
├── docs/                  report, architecture diagrams, screenshots, slides
├── docker/                Dockerfiles + docker-compose.yml
└── .github/workflows/     CI: build + test backend and frontend
```

---

## 11. Milestones

| Phase | What | Done when |
|---|---|---|
| 0. Setup | repo, toolchain, CMake skeleton, health endpoint, CI | `GET /api/v1/health` returns ok; CI green |
| 1. Data | metro network + interchanges + places; OSRM cache | `metro.json` has every line; spot-check 10 routes against DMRC |
| 2. Search core | graph, BFS/DFS/UCS/Greedy/A*, unit tests | A* = UCS cost on 100 random queries; tests pass |
| 3. Journeys | multi-modal edges, Yen's k-paths, Pareto + tags, `/journeys` | Bennett → Hauz Khas returns 3+ sensible mixed routes |
| 4. Fares | fare engine, adapters, collect samples, fit model, deep links | MAE per provider in a table; prices on every leg |
| 5. Frontend | Plan screen + map + route cards | full journey flow works in the browser |
| 6. Lab + extras | Algorithm Lab animation, disruptions, trip planner | all three demo-able |
| 7. Polish | Docker, deploy, report, slides, viva prep | one-command run; report draft done |

Stretch idea if the teacher wants adversarial search covered: "Rush Hour" mode, where an
adversary closes the segment that hurts your route most and the planner picks the route with the
best worst case (minimax over disruption choices, with α-β pruning).

---

## 12. Quality bar ("production grade")
- C++20, `-Wall -Wextra -Werror` in CI, clang-format, sanitizers (ASan/UBSan) in debug builds
- GoogleTest for every algorithm on hand-made graphs with known answers; property test A* ≡ UCS
- Config by JSON file + env vars, no hard-coded paths or keys; secrets in `.env` (git-ignored)
- Structured logging (spdlog), request IDs, timing in every response
- Caching: OSRM results in SQLite; graph loaded once at startup
- Docker multi-stage build; `docker compose up` runs everything
- GitHub Actions CI: backend build + tests, frontend lint + typecheck + build
- README with setup, architecture diagram, API docs, screenshots

---

## 13. For the report and viva
- Problem formulation as a state-space search (initial state, actions, transition model, goal
  test, path cost)
- Admissibility proof of the heuristic
- Experiment: 100 random origin/destination pairs × 5 algorithms → table + charts (nodes
  expanded, time, optimality gap)
- Hill climbing vs SA on 5/8/10-stop trips (mean cost over 30 runs, convergence plots)
- Fare model: dataset, regression, MAE per provider, limits of estimating prices
- Limitations + future work (live APIs, buses, real-time metro delays)
