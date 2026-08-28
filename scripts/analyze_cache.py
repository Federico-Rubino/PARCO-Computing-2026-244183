#!/usr/bin/env python3
#
# analyze_cache.py - parse Cachegrind logs from profile_cache.sh into a
# summary CSV of cache-miss rates.
#
# Usage:
#   python3 scripts/analyze_cache.py [log_dir=results/cache] [out_csv=results/cache/summary.csv]

import csv
import re
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

FNAME_DIJKSTRA = re.compile(r"^(?P<graph>.+)_dijkstra\.log$")
FNAME_DSA = re.compile(r"^(?P<graph>.+)_dsa_t(?P<threads>\d+)\.log$")


def parse_log(path):
    stats = {}
    text = path.read_text(errors="replace")
    for line in text.splitlines():
        line = re.sub(r"^==\d+==\s?", "", line).strip()
        for key, pattern in STAT_PATTERNS.items():
            if key in stats:
                continue
            m = re.match(pattern, line)
            if m:
                stats[key] = int(m.group(1).replace(",", ""))
    missing = [k for k in STAT_PATTERNS if k not in stats]
    if missing:
        raise ValueError(f"{path}: missing stats {missing} - not a valid cachegrind summary")
    return stats


def derive_rates(stats):
    return {
        "i1_miss_rate": stats["i1_misses"] / stats["i_refs"] if stats["i_refs"] else 0.0,
        "d1_miss_rate": stats["d1_misses"] / stats["d_refs"] if stats["d_refs"] else 0.0,
        "ll_miss_rate": stats["ll_misses"] / stats["ll_refs"] if stats["ll_refs"] else 0.0,
    }


def main():
    log_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("results/cache")
    out_csv = Path(sys.argv[2]) if len(sys.argv) > 2 else log_dir / "summary.csv"

    rows = []
    for path in sorted(log_dir.glob("*.log")):
        m = FNAME_DIJKSTRA.match(path.name)
        if m:
            graph, algo, threads = m.group("graph"), "dijkstra", 1
        else:
            m = FNAME_DSA.match(path.name)
            if not m:
                print(f"skip: {path.name} (unrecognized filename)", file=sys.stderr)
                continue
            graph, algo, threads = m.group("graph"), "dsa", int(m.group("threads"))

        try:
            stats = parse_log(path)
        except ValueError as e:
            print(f"skip: {e}", file=sys.stderr)
            continue

        row = {"graph": graph, "algorithm": algo, "threads": threads}
        row.update(stats)
        row.update(derive_rates(stats))
        rows.append(row)

    if not rows:
        sys.exit(f"no valid cachegrind logs found in {log_dir}")

    rows.sort(key=lambda r: (r["graph"], r["algorithm"], r["threads"]))

    fieldnames = ["graph", "algorithm", "threads",
                  "i_refs", "i1_misses", "i1_miss_rate", "lli_misses",
                  "d_refs", "d1_misses", "d1_miss_rate", "lld_misses",
                  "ll_refs", "ll_misses", "ll_miss_rate"]

    with open(out_csv, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    print(f"wrote {len(rows)} rows -> {out_csv}")
    for r in rows:
        print(f"  {r['graph']:<20} {r['algorithm']:<9} t={r['threads']:<3} "
              f"D1 miss={r['d1_miss_rate']*100:5.2f}%  LL miss={r['ll_miss_rate']*100:5.2f}%")


if __name__ == "__main__":
    main()
