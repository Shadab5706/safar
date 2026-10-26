"""Prefetch real road distances and drive times from OSRM (OpenStreetMap road network).

For every place it asks OSRM for the drive to:
  - its 10 nearest metro stations, plus the nearest station of every line within 35 km
    (so "cab to the Magenta Line" is an option even when it isn't the closest line)
  - every other place (for direct cab/auto/bike trips)

Output: data/processed/road_matrix.json
        [from, to, km, minutes]  with ids "p:<place>" / "s:<station>"
Cache:  data/raw/osrm_pairs.json  (re-runs only fetch what's missing)

OSRM times are free-flow (no traffic). The backend multiplies them by a time-of-day
traffic factor from backend/config/transport.json.

Respects the public server's usage policy: one request per second, results cached.

Usage:  python3 scripts/prefetch_road_matrix.py
"""

import datetime as dt
import json
import math
import pathlib
import sys
import time
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
METRO = ROOT / "data" / "processed" / "metro.json"
PLACES = ROOT / "data" / "processed" / "places.json"
CACHE = ROOT / "data" / "raw" / "osrm_pairs.json"
OUT = ROOT / "data" / "processed" / "road_matrix.json"

SERVERS = ["https://router.project-osrm.org", "https://routing.openstreetmap.de/routed-car"]
NEAREST_STATIONS = 10
PER_LINE_RADIUS_KM = 35
MAX_COORDS = 95  # stay under the public server's table size limit


def haversine_km(a, b):
    lat1, lng1, lat2, lng2 = map(math.radians, (a[0], a[1], b[0], b[1]))
    s = math.sin((lat2 - lat1) / 2) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin((lng2 - lng1) / 2) ** 2
    return 2 * 6371.0088 * math.asin(math.sqrt(s))


def table(sources, destinations):
    """sources/destinations: lists of (id, (lat, lng)). Returns {(src_id, dst_id): (km, min)}."""
    coords = sources + destinations
    path = ";".join(f"{lng:.6f},{lat:.6f}" for _, (lat, lng) in coords)
    q = (f"/table/v1/driving/{path}?annotations=duration,distance"
         f"&sources={';'.join(str(i) for i in range(len(sources)))}"
         f"&destinations={';'.join(str(i) for i in range(len(sources), len(coords)))}")
    last = None
    for attempt in range(4):
        for base in SERVERS:
            try:
                req = urllib.request.Request(base + q, headers={"User-Agent": "safar-student-project/0.1 (B.Tech AI course)"})
                with urllib.request.urlopen(req, timeout=60) as resp:
                    data = json.load(resp)
                time.sleep(1.1)
                if data.get("code") != "Ok":
                    raise RuntimeError(data.get("code"))
                out = {}
                for i, (sid, _) in enumerate(sources):
                    for j, (did, _) in enumerate(destinations):
                        d, t = data["distances"][i][j], data["durations"][i][j]
                        if d is not None and t is not None and sid != did:
                            out[(sid, did)] = (round(d / 1000, 2), round(t / 60, 1))
                return out
            except Exception as e:
                last = e
                time.sleep(2 + attempt * 3)
    raise SystemExit(f"OSRM failed after retries: {last}")


def main() -> None:
    metro = json.loads(METRO.read_text())
    places = json.loads(PLACES.read_text())["places"]
    stations = metro["stations"]
    cache = {tuple(k.split("|")): tuple(v) for k, v in json.loads(CACHE.read_text()).items()} if CACHE.exists() else {}

    def save_cache():
        CACHE.parent.mkdir(parents=True, exist_ok=True)
        CACHE.write_text(json.dumps({f"{a}|{b}": v for (a, b), v in cache.items()}))

    P = [(f"p:{p['id']}", (p["lat"], p["lng"])) for p in places]
    S = {s["id"]: (f"s:{s['id']}", (s["lat"], s["lng"])) for s in stations}

    # 1. place -> candidate stations
    for n, (pid, ppos) in enumerate(P, 1):
        by_dist = sorted(stations, key=lambda s: haversine_km(ppos, (s["lat"], s["lng"])))
        cands = {s["id"] for s in by_dist[:NEAREST_STATIONS]}
        seen_lines = set()
        for s in by_dist:
            if haversine_km(ppos, (s["lat"], s["lng"])) > PER_LINE_RADIUS_KM:
                break
            new = set(s["lines"]) - seen_lines
            if new:
                cands.add(s["id"])
                seen_lines |= new
        dests = [S[c] for c in sorted(cands) if (pid, S[c][0]) not in cache]
        if dests:
            print(f"  [{n}/{len(P)}] {pid}: {len(dests)} stations", flush=True)
            cache.update(table([(pid, ppos)], dests))
            save_cache()

    # 2. place <-> place
    half = MAX_COORDS // 3
    for i in range(0, len(P), half):
        src = P[i:i + half]
        for j in range(0, len(P), MAX_COORDS - half):
            dst = P[j:j + MAX_COORDS - half]
            if all((a, b) in cache for a, _ in src for b, _ in dst if a != b):
                continue
            print(f"  places {i}-{i + len(src)} x {j}-{j + len(dst)}", flush=True)
            cache.update(table(src, dst))
            save_cache()

    rows = sorted([a, b, km, mins] for (a, b), (km, mins) in cache.items())
    OUT.write_text(json.dumps({
        "meta": {
            "generated_at": dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds"),
            "source": "OSRM car profile on OpenStreetMap data (free-flow, no traffic)",
            "license": "Data (c) OpenStreetMap contributors, ODbL 1.0",
            "count": len(rows),
        },
        "pairs": rows,
    }, separators=(",", ":")))
    print(f"wrote {OUT.relative_to(ROOT)}: {len(rows)} pairs ({OUT.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    main()
