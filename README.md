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

## Rebuild the data

The processed data in `data/processed/` is committed, so this is only needed to refresh it.

```bash
python3 scripts/fetch_metro_osm.py      # download metro routes from OpenStreetMap
python3 scripts/build_metro_graph.py    # -> data/processed/metro.json
python3 scripts/build_places.py         # geocode data/curated/places.csv -> places.json
```

## API so far

| Endpoint | What it returns |
|---|---|
| `GET /api/v1/health` | status, version, loaded data counts |
| `GET /api/v1/places?q=hauz&limit=5` | autocomplete over places + metro stations |
| `GET /api/v1/metro/lines` | every line with colour, operator and station order |
| `GET /api/v1/metro/stations[/<id>]` | station coordinates and the lines serving them |
| `POST /api/v1/lab/compare` | run BFS / DFS / UCS / Greedy / A* on one station-to-station query: route, cost, optimality gap, nodes expanded, runtime, expansion order |

## Status

Phase 2 (search core) done: transit graph (616 nodes, 1384 edges) and five search algorithms with
38 tests, including A* matching UCS on 100 random real queries. Next: Phase 3 (multi-modal journeys
with cab/auto/walk). See PLAN.md §11.

Try it:

```bash
curl -s -X POST localhost:8080/api/v1/lab/compare \
  -d '{"from":"noida-sector-51","to":"kashmere-gate","include_expansion":false}'
```

Map data © OpenStreetMap contributors (ODbL).
