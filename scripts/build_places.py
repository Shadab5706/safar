"""Geocode the curated place list with OpenStreetMap Nominatim and write places.json.

Input:  data/curated/places.csv       (name, category, city, query)
        data/processed/metro.json     (to attach the nearest station to each place)
Output: data/processed/places.json
Cache:  data/raw/geocode_cache.json   (so re-runs don't hit Nominatim again)

Respects the Nominatim usage policy: one request per second, identifying User-Agent,
results cached. Places it cannot find are listed at the end. Fix their `query` in the CSV
and re-run.

Usage:  python3 scripts/build_places.py
"""

import csv
import json
import math
import pathlib
import re
import sys
import time
import urllib.parse
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
CSV = ROOT / "data" / "curated" / "places.csv"
METRO = ROOT / "data" / "processed" / "metro.json"
CACHE = ROOT / "data" / "raw" / "geocode_cache.json"
OUT = ROOT / "data" / "processed" / "places.json"

NOMINATIM = "https://nominatim.openstreetmap.org/search"
# left, top, right, bottom: Delhi-NCR incl. Meerut and Jewar
VIEWBOX = "76.70,29.10,77.85,28.05"
FAR_FROM_METRO_KM = 6.0  # reported for a manual look; not an error (e.g. Bennett, Jewar)


def haversine_km(a, b):
    lat1, lng1, lat2, lng2 = map(math.radians, (a[0], a[1], b[0], b[1]))
    s = math.sin((lat2 - lat1) / 2) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin((lng2 - lng1) / 2) ** 2
    return 2 * 6371.0088 * math.asin(math.sqrt(s))


def slug(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", text.lower().replace("'", "")).strip("-")


def geocode(query: str):
    params = urllib.parse.urlencode({
        "q": query, "format": "jsonv2", "limit": 1, "countrycodes": "in",
        "viewbox": VIEWBOX, "bounded": 1,
    })
    req = urllib.request.Request(f"{NOMINATIM}?{params}", headers={"User-Agent": "safar-student-project/0.1 (B.Tech AI course)"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        res = json.load(resp)
    time.sleep(1.1)
    return res[0] if res else None


def main() -> None:
    cache = json.loads(CACHE.read_text()) if CACHE.exists() else {}
    stations = json.loads(METRO.read_text())["stations"]
    rows = list(csv.DictReader(CSV.open()))

    places, missing, far = [], [], []
    for i, row in enumerate(rows, 1):
        tries = [row["query"], f"{row['name']}, {row['city']}"]
        hit = None
        for q in tries:
            if q not in cache:
                print(f"  [{i}/{len(rows)}] {q}", flush=True)
                try:
                    cache[q] = geocode(q)
                except Exception as e:
                    print(f"    failed: {e}", file=sys.stderr)
                    continue
                CACHE.parent.mkdir(parents=True, exist_ok=True)
                CACHE.write_text(json.dumps(cache))
            if cache[q]:
                hit = cache[q]
                break
        if not hit:
            missing.append(row["name"])
            continue

        pt = (float(hit["lat"]), float(hit["lon"]))
        near = min(stations, key=lambda s: haversine_km(pt, (s["lat"], s["lng"])))
        near_km = haversine_km(pt, (near["lat"], near["lng"]))
        place = {
            "id": slug(row["name"]),
            "name": row["name"],
            "category": row["category"],
            "city": row["city"],
            "lat": round(pt[0], 6),
            "lng": round(pt[1], 6),
            "nearest_station": {"id": near["id"], "distance_km": round(near_km, 2)},
            "osm": {"type": hit.get("osm_type"), "id": hit.get("osm_id"), "label": hit.get("display_name", "")[:120]},
        }
        places.append(place)
        if near_km > FAR_FROM_METRO_KM:
            far.append(f"{row['name']}: {near_km:.1f} km from {near['name']}")

    ids = [p["id"] for p in places]
    dupes = {x for x in ids if ids.count(x) > 1}
    if dupes:
        sys.exit(f"duplicate place ids: {sorted(dupes)}")

    OUT.write_text(json.dumps({
        "meta": {"source": "OpenStreetMap Nominatim", "license": "Data (c) OpenStreetMap contributors, ODbL 1.0",
                 "count": len(places)},
        "places": places,
    }, indent=1, ensure_ascii=False))

    print(f"wrote {OUT.relative_to(ROOT)}: {len(places)} places")
    if far:
        print(f"far from metro (> {FAR_FROM_METRO_KM} km), check these look right:")
        for f in far:
            print("  " + f)
    if missing:
        print(f"NOT FOUND ({len(missing)}): fix their query in places.csv and re-run:")
        for m in missing:
            print("  " + m)


if __name__ == "__main__":
    main()
