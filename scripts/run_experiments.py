"""Reproducible experiments for the project report.

Needs the backend running (default http://localhost:8080, override with SAFAR_URL).

  A. Algorithm comparison: 100 random station pairs x BFS, DFS, UCS, Greedy, A*
  B. Heuristic ablation: A* with the admissible speed vs slower (overestimating) speeds
  C. Journey planner: 30 random place pairs at 09:00 on a weekday

Output: docs/experiments/results.json + a printed summary.
Usage:  python3 scripts/run_experiments.py
"""

import json
import os
import pathlib
import random
import statistics as st
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
URL = os.environ.get("SAFAR_URL", "http://localhost:8080")
OUT = ROOT / "docs" / "experiments" / "results.json"
SEED = 42


def post(path, body):
    req = urllib.request.Request(URL + path, data=json.dumps(body).encode(), headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=60) as resp:
        return json.load(resp)


def get(path):
    with urllib.request.urlopen(URL + path, timeout=60) as resp:
        return json.load(resp)


def pairs(ids, n, rng):
    out = []
    while len(out) < n:
        a, b = rng.sample(ids, 2)
        out.append((a, b))
    return out


def main():
    rng = random.Random(SEED)
    stations = [s["id"] for s in get("/api/v1/metro/stations")["data"]]
    places = json.loads((ROOT / "data" / "processed" / "places.json").read_text())["places"]
    place_ids = [p["id"] for p in places]

    # ---- A. algorithm comparison ----
    algos = ["bfs", "dfs", "ucs", "greedy", "astar"]
    acc = {a: {"expanded": [], "runtime_us": [], "gap": [], "optimal": 0, "transfers": [], "minutes": []} for a in algos}
    station_pairs = pairs(stations, 100, rng)
    for a, b in station_pairs:
        d = post("/api/v1/lab/compare", {"from": a, "to": b, "include_expansion": False})["data"]
        for r in d["results"]:
            x = acc[r["algorithm"]]
            x["expanded"].append(r["stats"]["nodes_expanded"])
            x["runtime_us"].append(r["stats"]["runtime_us"])
            x["gap"].append(r["optimality_gap_pct"])
            x["optimal"] += r["optimality_gap_pct"] < 0.01
            x["transfers"].append(r["itinerary"]["transfers"])
            x["minutes"].append(r["itinerary"]["total_min"])
    algo_table = []
    for a in algos:
        x = acc[a]
        algo_table.append({
            "algorithm": a,
            "mean_nodes_expanded": round(st.mean(x["expanded"]), 1),
            "mean_runtime_us": round(st.mean(x["runtime_us"]), 1),
            "optimal_pct": x["optimal"],
            "mean_gap_pct": round(st.mean(x["gap"]), 1),
            "max_gap_pct": round(max(x["gap"]), 1),
            "mean_route_min": round(st.mean(x["minutes"]), 1),
            "mean_transfers": round(st.mean(x["transfers"]), 2),
        })

    # ---- B. heuristic ablation (A*) ----
    ablation = []
    for speed in [None, 60, 45, 30, 15]:
        expanded, gaps, optimal = [], [], 0
        for a, b in station_pairs:
            body = {"from": a, "to": b, "algorithms": ["astar"], "include_expansion": False}
            if speed:
                body["heuristic_speed_kmh"] = speed
            d = post("/api/v1/lab/compare", body)["data"]
            r = d["results"][0]
            expanded.append(r["stats"]["nodes_expanded"])
            gaps.append(r["optimality_gap_pct"])
            optimal += r["optimality_gap_pct"] < 0.01
        ablation.append({
            "heuristic_speed_kmh": speed or d["heuristic_speed_kmh"],
            "admissible": speed is None,
            "mean_nodes_expanded": round(st.mean(expanded), 1),
            "optimal_pct": optimal,
            "mean_gap_pct": round(st.mean(gaps), 2),
        })

    # ---- C. journey planner ----
    journeys = []
    for a, b in pairs(place_ids, 30, rng):
        body = post("/api/v1/journeys", {"from": a, "to": b, "depart_at": "2026-10-27T09:00"})
        d = body["data"]
        routes = d["routes"]
        kinds = lambda r: {l["kind"] for l in r["legs"]}
        tagged = lambda t: next(r for r in routes if t in r["tags"])
        journeys.append({
            "from": a, "to": b,
            "took_ms": body["meta"]["took_ms"],
            "routes": len(routes),
            "mixed": any({"road", "metro"} <= kinds(r) for r in routes),
            "fastest_min": tagged("fastest")["total_min"], "fastest_rs": tagged("fastest")["fare_rs"],
            "cheapest_min": tagged("cheapest")["total_min"], "cheapest_rs": tagged("cheapest")["fare_rs"],
            "recommended_min": tagged("recommended")["total_min"], "recommended_rs": tagged("recommended")["fare_rs"],
        })
    journey_summary = {
        "queries": len(journeys),
        "mean_routes": round(st.mean(j["routes"] for j in journeys), 2),
        "with_mixed_route_pct": round(100 * sum(j["mixed"] for j in journeys) / len(journeys), 1),
        "mean_latency_ms": round(st.mean(j["took_ms"] for j in journeys), 2),
        "max_latency_ms": round(max(j["took_ms"] for j in journeys), 2),
        "median_fastest_min": st.median(j["fastest_min"] for j in journeys),
        "median_fastest_rs": st.median(j["fastest_rs"] for j in journeys),
        "median_cheapest_min": st.median(j["cheapest_min"] for j in journeys),
        "median_cheapest_rs": st.median(j["cheapest_rs"] for j in journeys),
        "median_recommended_min": st.median(j["recommended_min"] for j in journeys),
        "median_recommended_rs": st.median(j["recommended_rs"] for j in journeys),
    }

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps({"seed": SEED, "algorithms": algo_table, "heuristic_ablation": ablation,
                               "journeys": journey_summary, "journey_queries": journeys}, indent=1))
    print(json.dumps({"algorithms": algo_table, "heuristic_ablation": ablation, "journeys": journey_summary}, indent=1))


if __name__ == "__main__":
    main()
