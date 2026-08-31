#!/usr/bin/env python3
#
# analyze_rr_compare.py - with/without Rabbit-reordering comparison table:
# wall-clock time and cache-miss rate, from profile_rr_compare.sh's output.
#
# Usage:
#   python3 scripts/analyze_rr_compare.py [rr_compare_dir=results/rr_compare] [out_csv=results/rr_compare/summary.csv]

import csv
import re
import statistics
import sys
from pathlib import Path

STAT_PATTERNS = {
    "i_refs":     r"^I\s+refs:\s+([\d,]+)",
    "i1_misses":  r"^I1\s+misses:\s+([\d,]+)",
    "lli_misses": r"^LLi misses:\s+([\d,]+)",
    "d_refs":     r"^D\s+refs:\s+([\d,]+)",
    "d1_misses":  r"^D1\s+misses:\s+([\d,]+)",
    "lld_misses": r"^LLd misses:\s+([\d,]+)",
    "ll_refs":    r"^LL refs:\s+([\d,]+)",
    "ll_misses":  r"^LL misses:\s+([\d,]+)",
}


def parse_cache_log(path):
    stats = {}
    for line in path.read_text(errors="replace").splitlines():
        line = re.sub(r"^==\d+==\s?", "", line).strip()
        for key, pattern in STAT_PATTERNS.items():
            if key in stats:
                continue
            m = re.match(pattern, line)
            if m:
                stats[key] = int(m.group(1).replace(",", ""))
    return stats


def cache_rates(stats):
    return {
        "d1_miss_rate": stats["d1_misses"] / stats["d_refs"] if stats.get("d_refs") else None,
        "ll_miss_rate": stats["ll_misses"] / stats["ll_refs"] if stats.get("ll_refs") else None,
    }


def mean_time(csv_path):
    times = []
    with open(csv_path) as f:
        for row in csv.DictReader(f):
            times.append(float(row["time_ms"]))
    return statistics.mean(times) if times else None


def collect(graph_dir, config):
    # config: "dijkstra" or "dsa_t<T>"
    result = {}
    for tag in ("rr", "norr"):
        csv_path = graph_dir / f"{config}_{tag}.csv"
        log_path = graph_dir / f"{config}_{tag}.log"
        entry = {}
        if csv_path.exists():
            entry["time_ms"] = mean_time(csv_path)
        if log_path.exists():
            stats = parse_cache_log(log_path)
            if stats:
                entry.update(cache_rates(stats))
        if entry:
            result[tag] = entry
    return result


def find_config(graph_dir, algo):
    for path in graph_dir.glob(f"{algo}_t*_rr.csv"):
        return path.stem.rsplit("_rr", 1)[0]  # e.g. "dsa_t32"
    for path in graph_dir.glob(f"{algo}_t*_rr.log"):
        return path.stem.rsplit("_rr", 1)[0]
    return None


def print_row(gname, config, entry):
    rr = entry.get("rr", {})
    norr = entry.get("norr", {})
    t_rr = rr.get("time_ms")
    t_norr = norr.get("time_ms")
    d1_rr = rr.get("d1_miss_rate")
    d1_norr = norr.get("d1_miss_rate")
    ll_rr = rr.get("ll_miss_rate")
    ll_norr = norr.get("ll_miss_rate")

    def fmt_ms(v):
        return f"{v:.3f}" if v is not None else "-"

    def fmt_pct(v):
        return f"{v*100:.2f}%" if v is not None else "-"

    print(f"{gname:<25}{config:<10}"
          f"{fmt_ms(t_norr):>12}{fmt_ms(t_rr):>12}"
          f"{fmt_pct(d1_norr):>12}{fmt_pct(d1_rr):>12}"
          f"{fmt_pct(ll_norr):>12}{fmt_pct(ll_rr):>12}")

    return {
        "graph": gname, "config": config,
        "time_ms_norr": t_norr, "time_ms_rr": t_rr,
        "d1_miss_norr": d1_norr, "d1_miss_rr": d1_rr,
        "ll_miss_norr": ll_norr, "ll_miss_rr": ll_rr,
    }


def main():
    rr_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("results/rr_compare")
    out_csv = Path(sys.argv[2]) if len(sys.argv) > 2 else rr_dir / "summary.csv"

    if not rr_dir.exists():
        sys.exit(f"missing {rr_dir}")

    header = (f"{'graph':<25}{'config':<10}{'time_norr':>12}{'time_rr':>12}"
              f"{'D1_norr':>12}{'D1_rr':>12}{'LL_norr':>12}{'LL_rr':>12}")
    print(header)
    print("-" * len(header))

    rows = []
    for graph_dir in sorted(p for p in rr_dir.iterdir() if p.is_dir()):
        gname = graph_dir.name

        dijkstra_entry = collect(graph_dir, "dijkstra")
        if dijkstra_entry:
            rows.append(print_row(gname, "dijkstra", dijkstra_entry))

        dsa_config = find_config(graph_dir, "dsa")
        if dsa_config:
            dsa_entry = collect(graph_dir, dsa_config)
            if dsa_entry:
                rows.append(print_row(gname, dsa_config, dsa_entry))

        wasp_config = find_config(graph_dir, "wasp")
        if wasp_config:
            wasp_entry = collect(graph_dir, wasp_config)
            if wasp_entry:
                rows.append(print_row(gname, wasp_config, wasp_entry))

    if not rows:
        sys.exit(f"no comparison data found under {rr_dir}")

    with open(out_csv, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print(f"\nwrote {out_csv}")


if __name__ == "__main__":
    main()
