#!/usr/bin/env python3
"""Fit per-product cab / auto / bike fare models from real app quotes.

Model, per product (Uber Go, Ola Mini, Rapido Bike, ...):

    quoted_fare / surge(hour) = base + per_km * km + per_min * minutes

surge(hour) is the product's time-of-day curve from providers.json, so peak-hour quotes
are deflated before fitting. Coefficients come from ordinary least squares with every
coefficient kept >= 0 (all subsets of the three terms are solved; the feasible one with
the lowest squared error wins). The Delhi auto meter is a government rate card and is
never refitted.

Input: data/fare_samples.csv with columns
    provider,product,from,to,km,minutes,hour,quoted_fare,date,source
km/minutes are the trip distance and duration the app shows, hour is 0-23, and source is
"app" for a quote read off the app or "example" for the illustrative rows shipped with the
repo. Example rows are ignored unless --include-examples is given, so they can never end
up in the real rate cards.

Output: a table (or --json report) with MAE before/after per product, and the fitted
values written back into backend/config/providers.json (basis = "fitted", plus a "fit"
block with the sample count, MAE and date). --dry-run only prints.

Standard library only; runs on the system python3.
"""
from __future__ import annotations

import argparse
import csv
import datetime as dt
import itertools
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
COLUMNS = ["provider", "product", "from", "to", "km", "minutes", "hour", "quoted_fare", "date", "source"]
TERMS = ["base", "per_km", "per_min"]
MODEL = "fare / surge(hour) = base + per_km*km + per_min*minutes  (least squares, coefficients >= 0)"
RECOMMENDED_SAMPLES = 8


class DataError(Exception):
    pass


def load_samples(path: Path, include_examples: bool):
    rows, skipped_examples, problems = [], 0, []
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        missing = [c for c in COLUMNS if c not in (reader.fieldnames or [])]
        if missing:
            raise DataError(f"{path}: missing columns {', '.join(missing)}")
        for line, r in enumerate(reader, start=2):
            if not any((v or "").strip() for v in r.values()):
                continue
            if r["source"].strip() == "example" and not include_examples:
                skipped_examples += 1
                continue
            try:
                row = {
                    "provider": r["provider"].strip(),
                    "product": r["product"].strip(),
                    "km": float(r["km"]),
                    "minutes": float(r["minutes"]),
                    "hour": int(r["hour"]),
                    "fare": float(r["quoted_fare"]),
                    "line": line,
                }
            except ValueError as e:
                problems.append(f"line {line}: {e}")
                continue
            if row["km"] <= 0 or row["minutes"] <= 0 or row["fare"] <= 0 or not 0 <= row["hour"] <= 23:
                problems.append(f"line {line}: km, minutes and quoted_fare must be > 0 and hour 0-23")
                continue
            rows.append(row)
    if problems:
        raise DataError(f"{path}:\n  " + "\n  ".join(problems))
    return rows, skipped_examples


def multiplier(provider: dict, card: dict, hour: int) -> float:
    surge = card.get("surge_by_hour", provider.get("surge_by_hour", [1.0] * 24))[hour]
    night_pct = card.get("night_pct", 0)
    lo, hi = card.get("night_from", 23), card.get("night_to", 5)
    night = night_pct > 0 and ((lo <= hour < hi) if lo <= hi else (hour >= lo or hour < hi))
    return surge * (1 + night_pct / 100 if night else 1)


def predict(provider: dict, card: dict, row: dict) -> float:
    """Same formula as RateCardProvider::fare (no traffic-waiting term for app products)."""
    metered = (card.get("base", 0) + card.get("per_km", 0) * max(0.0, row["km"] - card.get("base_km", 0))
               + card.get("per_min", 0) * row["minutes"])
    return max(card.get("min_fare", 0), metered) * multiplier(provider, card, row["hour"])


def solve(a: list[list[float]], b: list[float]):
    """Gaussian elimination with partial pivoting; None if singular."""
    n = len(b)
    m = [row[:] + [b[i]] for i, row in enumerate(a)]
    for col in range(n):
        piv = max(range(col, n), key=lambda r: abs(m[r][col]))
        if abs(m[piv][col]) < 1e-9:
            return None
        m[col], m[piv] = m[piv], m[col]
        for r in range(n):
            if r != col:
                f = m[r][col] / m[col][col]
                m[r] = [x - f * y for x, y in zip(m[r], m[col])]
    return [m[i][n] / m[i][i] for i in range(n)]


def fit_nonnegative(xs: list[list[float]], ys: list[float]):
    """Least squares over every subset of terms; returns the best feasible coefficient list."""
    best, best_sse = None, float("inf")
    for k in range(1, len(TERMS) + 1):
        for subset in itertools.combinations(range(len(TERMS)), k):
            if subset == (0,):
                continue  # a flat fare explains nothing about distance or time
            ata = [[sum(x[i] * x[j] for x in xs) for j in subset] for i in subset]
            aty = [sum(x[i] * y for x, y in zip(xs, ys)) for i in subset]
            sol = solve(ata, aty)
            if sol is None or any(c < -1e-9 for c in sol):
                continue
            coef = [0.0] * len(TERMS)
            for i, c in zip(subset, sol):
                coef[i] = max(0.0, c)
            sse = sum((sum(c * xi for c, xi in zip(coef, x)) - y) ** 2 for x, y in zip(xs, ys))
            if sse < best_sse - 1e-9:
                best, best_sse = coef, sse
    return best


def mae(provider: dict, card: dict, rows: list[dict]) -> float:
    return sum(abs(predict(provider, card, r) - r["fare"]) for r in rows) / len(rows)


def mape(provider: dict, card: dict, rows: list[dict]) -> float:
    return 100 * sum(abs(predict(provider, card, r) - r["fare"]) / r["fare"] for r in rows) / len(rows)


def run(args) -> dict:
    config = json.loads(args.providers.read_text(encoding="utf-8"))
    rows, skipped_examples = load_samples(args.samples, args.include_examples)
    report = {"model": MODEL, "samples": str(args.samples), "used_rows": len(rows),
              "skipped_example_rows": skipped_examples, "products": [], "skipped": [], "wrote": None}

    groups: dict[tuple[str, str], list[dict]] = {}
    for r in rows:
        groups.setdefault((r["provider"], r["product"]), []).append(r)

    for (pid, prod), group in sorted(groups.items()):
        provider = config.get("providers", {}).get(pid)
        card = (provider or {}).get("products", {}).get(prod)
        skip = {"provider": pid, "product": prod, "n": len(group)}
        if card is None:
            report["skipped"].append({**skip, "reason": "not in providers.json"})
            continue
        if provider.get("adapter") == "meter" or card.get("basis") == "govt_rate_card":
            report["skipped"].append({**skip, "reason": "government rate card, not refitted"})
            continue
        if len(group) < args.min_samples:
            report["skipped"].append({**skip, "reason": f"need at least {args.min_samples} samples"})
            continue
        xs = [[1.0, r["km"], r["minutes"]] for r in group]
        ys = [r["fare"] / multiplier(provider, card, r["hour"]) for r in group]
        coef = fit_nonnegative(xs, ys)
        if coef is None:
            report["skipped"].append({**skip, "reason": "samples too similar to separate km from minutes"})
            continue

        before = mae(provider, card, group)
        fitted = dict(card)
        fitted.update({t: round(c, 2) for t, c in zip(TERMS, coef)})
        fitted["base_km"] = 0
        after, after_pct = mae(provider, fitted, group), mape(provider, fitted, group)
        fitted["basis"] = "fitted"
        fitted["range_pct"] = round(min(0.5, max(0.05, after_pct / 100)), 2)
        fitted["fit"] = {"samples": len(group), "mae_rs": round(after, 2), "mape_pct": round(after_pct, 1),
                         "fitted_on": args.today, "includes_examples": args.include_examples}
        provider["products"][prod] = fitted
        report["products"].append({"provider": pid, "product": prod, "n": len(group),
                                   **{t: fitted[t] for t in TERMS},
                                   "mae_before_rs": round(before, 2), "mae_after_rs": round(after, 2),
                                   "mape_pct": round(after_pct, 1), "few_samples": len(group) < RECOMMENDED_SAMPLES})

    if report["products"] and not args.dry_run:
        args.providers.write_text(dump(config) + "\n", encoding="utf-8")
        report["wrote"] = str(args.providers)
    return report


def dump(obj, indent: int = 0) -> str:
    """JSON with short objects and number arrays kept on one line, like the hand-written file."""
    flat = json.dumps(obj, ensure_ascii=False, separators=(", ", ": "))
    if not isinstance(obj, (dict, list)) or len(flat) + indent <= 250 or (
            isinstance(obj, list) and all(isinstance(x, (int, float)) for x in obj)):
        return flat
    pad, inner = " " * indent, " " * (indent + 2)
    if isinstance(obj, list):
        return "[\n" + ",\n".join(inner + dump(x, indent + 2) for x in obj) + "\n" + pad + "]"
    items = [f"{inner}{json.dumps(k)}: {dump(v, indent + 2)}" for k, v in obj.items()]
    return "{\n" + ",\n".join(items) + "\n" + pad + "}"


def print_table(report: dict) -> None:
    print("Fare model fit:", report["model"])
    print(f"samples: {report['used_rows']} used, {report['skipped_example_rows']} example rows skipped")
    if report["products"]:
        print()
        print(f"{'product':<18}{'n':>4}{'base':>9}{'per_km':>9}{'per_min':>9}{'MAE before':>13}{'MAE after':>12}{'MAPE':>8}")
        for p in report["products"]:
            name = f"{p['provider']}/{p['product']}"
            flag = "  (few samples)" if p["few_samples"] else ""
            print(f"{name:<18}{p['n']:>4}{p['base']:>9.2f}{p['per_km']:>9.2f}{p['per_min']:>9.2f}"
                  f"{'Rs ' + format(p['mae_before_rs'], '.2f'):>13}{'Rs ' + format(p['mae_after_rs'], '.2f'):>12}"
                  f"{format(p['mape_pct'], '.1f') + '%':>8}{flag}")
    for s in report["skipped"]:
        plural = "" if s["n"] == 1 else "s"
        print(f"skipped: {s['provider']}/{s['product']} ({s['n']} sample{plural}): {s['reason']}")
    if not report["products"]:
        hint = " (pass --include-examples to try the pipeline on the example rows)" if report["skipped_example_rows"] else ""
        print(f"nothing fitted{hint}")
    elif report["wrote"]:
        print(f"wrote {report['wrote']} ({len(report['products'])} products refitted)")
    else:
        print("dry run: providers.json not changed")


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--samples", type=Path, default=ROOT / "data" / "fare_samples.csv")
    ap.add_argument("--providers", type=Path, default=ROOT / "backend" / "config" / "providers.json")
    ap.add_argument("--include-examples", action="store_true", help="also fit the illustrative example rows")
    ap.add_argument("--min-samples", type=int, default=3, help="fewest quotes a product needs (default 3)")
    ap.add_argument("--dry-run", action="store_true", help="print the fit, do not write providers.json")
    ap.add_argument("--json", action="store_true", help="print a JSON report instead of the table")
    ap.add_argument("--today", default=dt.date.today().isoformat(), help=argparse.SUPPRESS)
    args = ap.parse_args(argv)
    if args.min_samples < len(TERMS):
        ap.error(f"--min-samples must be at least {len(TERMS)} (one per coefficient)")
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", args.today):
        ap.error("--today must be YYYY-MM-DD")
    try:
        report = run(args)
    except (DataError, OSError, json.JSONDecodeError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print_table(report)
    return 0


if __name__ == "__main__":
    sys.exit(main())
