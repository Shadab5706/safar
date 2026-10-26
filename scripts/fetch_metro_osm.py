"""Download every metro route in Delhi-NCR from OpenStreetMap (Overpass API).

Output: data/raw/osm_metro.json (raw Overpass response, git-ignored).
Data (c) OpenStreetMap contributors, ODbL — credited in the app's About page.

Usage:  python3 scripts/fetch_metro_osm.py
"""

import json
import pathlib
import sys
import urllib.parse
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "data" / "raw" / "osm_metro.json"

# south, west, north, east — covers Delhi, Noida, Greater Noida, Gurugram, Faridabad, Ghaziabad, Bahadurgarh
BBOX = "28.30,76.80,28.90,77.60"

QUERY = f"""
[out:json][timeout:180];
rel["route"~"^(subway|light_rail|monorail|train)$"]["network"~"Metro|DMRC|NMRC|Rapid",i]({BBOX})->.routes;
.routes out body;
node(r.routes)->.stops;
.stops out body;
"""

ENDPOINTS = [
    "https://overpass-api.de/api/interpreter",
    "https://overpass.kumi.systems/api/interpreter",
]


def fetch() -> dict:
    data = urllib.parse.urlencode({"data": QUERY}).encode()
    last_err = None
    for url in ENDPOINTS:
        try:
            req = urllib.request.Request(url, data=data, headers={"User-Agent": "safar-student-project/0.1"})
            with urllib.request.urlopen(req, timeout=240) as resp:
                return json.load(resp)
        except Exception as e:  # try the next mirror
            last_err = e
            print(f"  {url} failed: {e}", file=sys.stderr)
    raise SystemExit(f"all Overpass endpoints failed: {last_err}")


def main() -> None:
    print("querying Overpass…")
    result = fetch()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(result))
    els = result.get("elements", [])
    rels = [e for e in els if e["type"] == "relation"]
    nodes = [e for e in els if e["type"] == "node"]
    print(f"saved {OUT.relative_to(ROOT)}: {len(rels)} routes, {len(nodes)} nodes")


if __name__ == "__main__":
    main()
