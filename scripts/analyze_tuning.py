#!/usr/bin/env python3
#
# analyze_tuning.py - summarize a delta/schedule tuning sweep from
# tune_params.sh and report the winning configuration by mean time.
#
# Usage:
#   python3 scripts/analyze_tuning.py <tuning_dir> <graph_name> [delta|schedule|all]
#
#   'delta' and 'schedule' print only the winning value, for shell capture.
#   'all' (default) prints the full human-readable report.

import csv
import re
import statistics
import sys
from pathlib import Path

DELTA_RE = re.compile(r"^(?P<graph>.+)_delta(?P<delta>[0-9.]+)\.csv$")
SCHED_RE = re.compile(r"^(?P<graph>.+)_sched(?P<sched>.+)\.csv$")


def read_times(path):
    times = []
    with open(path) as f:
        for row in csv.DictReader(f):
            times.append(float(row["time_ms"]))
    return times


def mean_time(path):
    times = read_times(path)
    mean = statistics.mean(times)
    std = statistics.stdev(times) if len(times) > 1 else 0.0
    return mean, std, len(times)


def collect_deltas(tuning_dir, gname):
    results = {}
    for path in sorted(tuning_dir.glob(f"{gname}_delta*.csv")):
        m = DELTA_RE.match(path.name)
        if not m:
            continue
        results[m.group("delta")] = mean_time(path)
    return results


def collect_scheds(tuning_dir, gname):
    results = {}
    for path in sorted(tuning_dir.glob(f"{gname}_sched*.csv")):
        m = SCHED_RE.match(path.name)
        if not m:
            continue
        sched = m.group("sched").replace("_", ",")
        results[sched] = mean_time(path)
    return results


def main():
    if len(sys.argv) < 3:
        sys.exit("usage: analyze_tuning.py <tuning_dir> <graph_name> [delta|schedule|all]")
    tuning_dir = Path(sys.argv[1])
    gname = sys.argv[2]
    phase = sys.argv[3] if len(sys.argv) > 3 else "all"

    deltas = collect_deltas(tuning_dir, gname)
    scheds = collect_scheds(tuning_dir, gname)

    if phase == "delta":
        if not deltas:
            sys.exit(f"no delta sweep results found for {gname} in {tuning_dir}")
        print(min(deltas, key=lambda k: deltas[k][0]))
        return

    if phase == "schedule":
        if not scheds:
            sys.exit(f"no schedule sweep results found for {gname} in {tuning_dir}")
        print(min(scheds, key=lambda k: scheds[k][0]))
        return

    row = "{:<12}{:>4}{:>12.4f}{:>10.4f}"
    if deltas:
        print(f"\n== {gname}: delta sweep ==")
        print(f"{'delta':<12}{'n':>4}{'mean_ms':>12}{'std_ms':>10}")
        for k in sorted(deltas, key=float):
            mean, std, n = deltas[k]
            print(row.format(k, n, mean, std))
        best_delta = min(deltas, key=lambda k: deltas[k][0])
        print(f"best delta: {best_delta} ({deltas[best_delta][0]:.4f} ms)")

    if scheds:
        print(f"\n== {gname}: schedule sweep ==")
        print(f"{'schedule':<12}{'n':>4}{'mean_ms':>12}{'std_ms':>10}")
        for k, (mean, std, n) in scheds.items():
            print(row.format(k, n, mean, std))
        best_sched = min(scheds, key=lambda k: scheds[k][0])
        print(f"best schedule: {best_sched} ({scheds[best_sched][0]:.4f} ms)")

    if not deltas and not scheds:
        sys.exit(f"no tuning results found for {gname} in {tuning_dir}")


if __name__ == "__main__":
    main()
