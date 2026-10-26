"""Turn the raw OpenStreetMap metro download into Safar's metro network.

Input:  data/raw/osm_metro.json          (from fetch_metro_osm.py)
        data/curated/interchanges.json   (hand-curated transfer times)
Output: data/processed/metro.json

What it does
  1. Groups OSM route relations into services (both directions of one service = one entry).
  2. Merges stop positions into stations by normalised name ("Azadpur (Pink Line)" -> "Azadpur").
  3. Builds ride segments between consecutive stations with distance + modelled travel time.
  4. Adds transfers: between lines inside one station, and short walking links between nearby
     stations with different names (e.g. Noida Sector 52 <-> Noida Sector 51).
  5. Validates: every service has >= 2 stations, the whole network is one connected graph.

Usage:  python3 scripts/build_metro_graph.py
"""

import collections
import datetime as dt
import json
import math
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
RAW = ROOT / "data" / "raw" / "osm_metro.json"
CURATED = ROOT / "data" / "curated" / "interchanges.json"
OUT = ROOT / "data" / "processed" / "metro.json"

# Running speed (km/h between stations) and dwell (min per stop) per mode, calibrated so that
# end-to-end times match published journey times (e.g. Blue Line Dwarka 21 -> Noida EC ~95 min,
# Airport Express New Delhi -> Dwarka 21 ~21 min, Namo Bharat Sarai Kale Khan -> Modipuram ~60 min).
MODE_PARAMS = {
    "metro":           {"speed_kmh": 45,  "dwell_min": 0.4,  "avg_wait_min": 2.5},
    "airport_express": {"speed_kmh": 80,  "dwell_min": 0.5,  "avg_wait_min": 5.0},
    "rrts":            {"speed_kmh": 110, "dwell_min": 0.75, "avg_wait_min": 7.5},
}
# Lines that run less often than the default for their mode.
WAIT_OVERRIDES = {"grey": 5.0, "aqua": 4.0, "rapid-metro-gurugram": 3.0}

OPERATORS = {
    "Delhi Metro": "DMRC",
    "Noida Metro": "NMRC",
    "Rapid Metro Gurgaon": "RAPID",
    "RapidX": "NCRTC",
}
COLOR_FIXES = {"aqua": "#00B5E2", "gray": "#5E7FA6"}
TRACK_DETOUR = 1.15  # track length vs straight line between neighbouring stations


def haversine_km(a, b):
    lat1, lng1, lat2, lng2 = map(math.radians, (a[0], a[1], b[0], b[1]))
    s = math.sin((lat2 - lat1) / 2) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin((lng2 - lng1) / 2) ** 2
    return 2 * 6371.0088 * math.asin(math.sqrt(s))


def display_name(raw: str) -> str:
    name = re.sub(r"\s*\((?:[A-Za-z]+ Line|Line \d+|Aqua Line|RRTS|Namo Bharat)\)\s*$", "", raw).strip()
    name = re.sub(r"\s*-\s*", " - ", name)  # "Maujpur-Babarpur" and "Maujpur - Babarpur" -> one form
    return name


def slug(text: str) -> str:
    text = text.lower().replace("&", " and ")
    text = re.sub(r"[^a-z0-9]+", "-", text)
    return text.strip("-")


def line_base(rel_name: str) -> str:
    base = re.split(r"\s*[(:]", rel_name, maxsplit=1)[0].strip()
    return {"Rapid MetroRail Gurgaon": "Rapid Metro Gurugram", "Delhi–Meerut RRTS": "Namo Bharat (RRTS)"}.get(base, base)


def main() -> None:
    raw = json.loads(RAW.read_text())
    cur = json.loads(CURATED.read_text())
    nodes = {e["id"]: e for e in raw["elements"] if e["type"] == "node"}
    rels = [e for e in raw["elements"] if e["type"] == "relation"]

    # ---- 1. stations: merge stop positions by normalised name ----
    station_pts = collections.defaultdict(list)    # station id -> [(lat, lng)]
    station_names = collections.defaultdict(collections.Counter)

    def station_of(node_id):
        n = nodes[node_id]
        name = display_name(n["tags"]["name"])
        sid = slug(name)
        station_pts[sid].append((n["lat"], n["lon"]))
        station_names[sid][name] += 1
        return sid

    # ---- 2. services: group both directions of a route ----
    groups = collections.OrderedDict()
    for r in rels:
        tags = r["tags"]
        seq = [station_of(m["ref"]) for m in r["members"]
               if m["type"] == "node" and m["role"].startswith("stop") and m["ref"] in nodes]
        seq = [s for i, s in enumerate(seq) if i == 0 or s != seq[i - 1]]  # drop repeated stops
        if len(seq) < 2:
            continue
        base = line_base(tags.get("name", ""))
        key = (base, frozenset((seq[0], seq[-1])))
        g = groups.setdefault(key, {"base": base, "tags": tags, "seq": seq, "pairs": set()})
        if len(seq) > len(g["seq"]):
            g["seq"] = seq
        for a, b in zip(seq, seq[1:]):
            g["pairs"].add(frozenset((a, b)))

    # Longest service of each base line keeps the plain id; the others become branches.
    by_base = collections.defaultdict(list)
    for g in groups.values():
        by_base[g["base"]].append(g)

    lines, segments = [], []
    for base, gs in by_base.items():
        gs.sort(key=lambda g: -len(g["seq"]))
        for i, g in enumerate(gs):
            tags = g["tags"]
            line_id = slug(base.replace(" Line", "")) + ("" if i == 0 else f"-branch{'' if len(gs) == 2 else i}")
            mode = "rrts" if "RRTS" in base else "airport_express" if "Airport" in base else "metro"
            p = MODE_PARAMS[mode]
            color = tags.get("colour", "#888888")
            color = COLOR_FIXES.get(color.lower(), color).upper()
            seq = g["seq"]
            lines.append({
                "id": line_id,
                "name": base if i == 0 else f"{base} ({station_names[seq[0]].most_common(1)[0][0]} – {station_names[seq[-1]].most_common(1)[0][0]})",
                "color": color,
                "operator": OPERATORS.get(tags.get("network"), tags.get("network")),
                "mode": mode,
                "avg_wait_min": WAIT_OVERRIDES.get(line_id, p["avg_wait_min"]),
                "stations": seq,
            })
            g["line_id"], g["mode"] = line_id, mode

    # Station coordinates = mean of all its stop positions.
    stations = {}
    for sid, pts in station_pts.items():
        stations[sid] = {
            "id": sid,
            "name": station_names[sid].most_common(1)[0][0],
            "lat": round(sum(p[0] for p in pts) / len(pts), 6),
            "lng": round(sum(p[1] for p in pts) / len(pts), 6),
            "lines": [],
        }
    for ln in lines:
        for sid in dict.fromkeys(ln["stations"]):
            stations[sid]["lines"].append(ln["id"])

    # ---- 3. ride segments ----
    for g in groups.values():
        p = MODE_PARAMS[g["mode"]]
        for pair in sorted(g["pairs"], key=sorted):
            a, b = sorted(pair)
            d = haversine_km((stations[a]["lat"], stations[a]["lng"]), (stations[b]["lat"], stations[b]["lng"])) * TRACK_DETOUR
            segments.append({
                "line": g["line_id"], "a": a, "b": b,
                "distance_km": round(d, 3),
                "time_min": round(d / p["speed_kmh"] * 60 + p["dwell_min"], 2),
            })

    # ---- 4. transfers ----
    transfers = []
    for s in stations.values():
        if len(s["lines"]) > 1:
            transfers.append({"type": "in_station", "station": s["id"],
                              "walk_min": cur["station_transfer_min"].get(s["id"], cur["default_transfer_min"])})
    ids = sorted(stations)
    for i, a in enumerate(ids):
        for b in ids[i + 1:]:
            sa, sb = stations[a], stations[b]
            if set(sa["lines"]) & set(sb["lines"]):
                continue  # they share a line: the train already connects them
            d_km = haversine_km((sa["lat"], sa["lng"]), (sb["lat"], sb["lng"]))
            if d_km * 1000 <= cur["max_walk_link_m"]:
                walk = d_km * cur["walk_detour_factor"] / cur["walk_speed_kmh"] * 60 + cur["walk_link_overhead_min"]
                transfers.append({"type": "walk_link", "a": a, "b": b,
                                  "distance_km": round(d_km * cur["walk_detour_factor"], 3),
                                  "walk_min": round(walk, 1)})

    for x in cur.get("extra_walk_links", []):
        if x["a"] not in stations or x["b"] not in stations:
            sys.exit(f"extra_walk_links: unknown station in {x}")
        transfers.append({"type": "walk_link", "a": x["a"], "b": x["b"],
                          "distance_km": x["distance_km"], "walk_min": x["walk_min"]})

    # ---- 5. validate ----
    adj = collections.defaultdict(set)
    for sg in segments:
        adj[sg["a"]].add(sg["b"]); adj[sg["b"]].add(sg["a"])
    for t in transfers:
        if t["type"] == "walk_link":
            adj[t["a"]].add(t["b"]); adj[t["b"]].add(t["a"])
    seen, stack = set(), [next(iter(stations))]
    while stack:
        s = stack.pop()
        if s in seen:
            continue
        seen.add(s); stack.extend(adj[s] - seen)
    unreachable = sorted(set(stations) - seen)

    out = {
        "meta": {
            "generated_at": dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds"),
            "source": "OpenStreetMap (Overpass API)",
            "license": "Data (c) OpenStreetMap contributors, ODbL 1.0",
            "counts": {"lines": len(lines), "stations": len(stations), "segments": len(segments), "transfers": len(transfers)},
        },
        "lines": lines,
        "stations": sorted(stations.values(), key=lambda s: s["id"]),
        "segments": segments,
        "transfers": transfers,
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(out, indent=1, ensure_ascii=False))

    print(f"wrote {OUT.relative_to(ROOT)}: {out['meta']['counts']}")
    for ln in lines:
        km = sum(s["distance_km"] for s in segments if s["line"] == ln["id"])
        mins = sum(s["time_min"] for s in segments if s["line"] == ln["id"])
        print(f"  {ln['id']:<24} {len(dict.fromkeys(ln['stations'])):>3} stns {km:6.1f} km {mins:6.1f} min  "
              f"{stations[ln['stations'][0]]['name']} -> {stations[ln['stations'][-1]]['name']}")
    if unreachable:
        print(f"WARNING: {len(unreachable)} stations not connected to the rest: {unreachable}", file=sys.stderr)
        sys.exit(1)
    print("network is fully connected")


if __name__ == "__main__":
    main()
