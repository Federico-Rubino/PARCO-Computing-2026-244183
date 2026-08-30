#!/usr/bin/env python3
#
# plot_results.py - summarize and plot strong-scaling results for one graph.
# Reads the CSVs produced by run_experiments.sh (results/bench/), reports
# mean/std/median/p90 per thread count, and draws time/speedup/efficiency
# vs threads.
#
# Usage:
#   python3 scripts/plot_results.py <graph_name> [bench_dir=results/bench] [out_dir=results/plots]

import csv
import math
import statistics
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

COLOR_MEASURED = "#2a78d6"
COLOR_REFERENCE = "#898781"
COLOR_INK = "#0b0b0b"
COLOR_GRID = "#e1e0d9"


def read_times(path):
    times = []
    with open(path) as f:
        for row in csv.DictReader(f):
            times.append(float(row["time_ms"]))
    return times


def percentile(values, p):
    values = sorted(values)
    idx = math.ceil(p / 100 * len(values)) - 1
    idx = max(0, min(idx, len(values) - 1))
    return values[idx]


def summarize(times):
    return {
        "n": len(times),
        "mean": statistics.mean(times),
        "std": statistics.stdev(times) if len(times) > 1 else 0.0,
        "median": statistics.median(times),
        "min": min(times),
        "p90": percentile(times, 90),
    }


def load_dijkstra(bench_dir):
    path = bench_dir / "dijkstra.csv"
    if not path.exists():
        sys.exit(f"missing {path}")
    return summarize(read_times(path))


def load_dsa(bench_dir):
    per_thread = {}
    for path in sorted(bench_dir.glob("dsa_t*.csv")):
        threads = int(path.stem.rsplit("_t", 1)[1])
        per_thread[threads] = summarize(read_times(path))
    if not per_thread:
        sys.exit(f"no dsa_t*.csv files found in {bench_dir}")
    return dict(sorted(per_thread.items()))


def print_table(gname, dijkstra_stats, dsa_stats):
    header = f"{'config':<12}{'n':>4}{'mean_ms':>12}{'std_ms':>10}{'median_ms':>12}{'p90_ms':>10}"
    print(f"\n== {gname} ==")
    print(header)
    row = "{:<12}{:>4}{:>12.4f}{:>10.4f}{:>12.4f}{:>10.4f}"
    s = dijkstra_stats
    print(row.format("dijkstra", s["n"], s["mean"], s["std"], s["median"], s["p90"]))
    for t, s in dsa_stats.items():
        print(row.format(f"dsa t={t}", s["n"], s["mean"], s["std"], s["median"], s["p90"]))


def write_summary_csv(out_path, gname, dijkstra_stats, dsa_stats):
    with open(out_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["graph", "config", "threads", "n", "mean_ms", "std_ms", "median_ms", "min_ms", "p90_ms"])
        s = dijkstra_stats
        writer.writerow([gname, "dijkstra", 1, s["n"], s["mean"], s["std"], s["median"], s["min"], s["p90"]])
        for t, s in dsa_stats.items():
            writer.writerow([gname, "dsa", t, s["n"], s["mean"], s["std"], s["median"], s["min"], s["p90"]])


def style_axes(ax):
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_color(COLOR_GRID)
    ax.spines["bottom"].set_color(COLOR_GRID)
    ax.grid(True, color=COLOR_GRID, linewidth=0.8, zorder=0)
    ax.tick_params(colors=COLOR_INK)


def plot_time(gname, dsa_stats, out_dir):
    threads = list(dsa_stats.keys())
    means = [dsa_stats[t]["mean"] for t in threads]
    stds = [dsa_stats[t]["std"] for t in threads]

    fig, ax = plt.subplots(figsize=(5, 3.5))
    ax.errorbar(threads, means, yerr=stds, color=COLOR_MEASURED, marker="o",
                markersize=6, linewidth=1.8, capsize=3, label="Delta-stepping")
    ax.set_xscale("log", base=2)
    ax.set_xticks(threads)
    ax.set_xticklabels(threads)
    ax.set_xlabel("threads")
    ax.set_ylabel("time (ms)")
    ax.set_title(f"{gname}: wall-clock time vs threads")
    style_axes(ax)
    ax.legend(frameon=False)
    fig.tight_layout()
    fig.savefig(out_dir / "time.png", dpi=200)
    plt.close(fig)


def plot_speedup(gname, dijkstra_stats, dsa_stats, out_dir):
    threads = list(dsa_stats.keys())
    t1 = dijkstra_stats["mean"]
    speedup = [t1 / dsa_stats[t]["mean"] for t in threads]

    fig, ax = plt.subplots(figsize=(5, 3.5))
    ax.plot(threads, threads, color=COLOR_REFERENCE, linestyle="--", linewidth=1.5, label="ideal")
    ax.plot(threads, speedup, color=COLOR_MEASURED, marker="o", markersize=6,
            linewidth=1.8, label="Delta-stepping")
    ax.set_xscale("log", base=2)
    ax.set_yscale("log", base=2)
    ax.set_xticks(threads)
    ax.set_xticklabels(threads)
    ax.set_xlabel("threads")
    ax.set_ylabel("speedup (vs. Dijkstra T1)")
    ax.set_title(f"{gname}: strong-scaling speedup")
    style_axes(ax)
    ax.legend(frameon=False)
    fig.tight_layout()
    fig.savefig(out_dir / "speedup.png", dpi=200)
    plt.close(fig)


def plot_efficiency(gname, dijkstra_stats, dsa_stats, out_dir):
    threads = list(dsa_stats.keys())
    t1 = dijkstra_stats["mean"]
    efficiency = [(t1 / dsa_stats[t]["mean"]) / t for t in threads]

    fig, ax = plt.subplots(figsize=(5, 3.5))
    ax.axhline(1.0, color=COLOR_REFERENCE, linestyle="--", linewidth=1.5, label="ideal")
    ax.plot(threads, efficiency, color=COLOR_MEASURED, marker="o", markersize=6,
            linewidth=1.8, label="Delta-stepping")
    ax.set_xscale("log", base=2)
    ax.set_xticks(threads)
    ax.set_xticklabels(threads)
    ax.set_ylim(0, 1.15)
    ax.set_xlabel("threads")
    ax.set_ylabel("parallel efficiency")
    ax.set_title(f"{gname}: parallel efficiency")
    style_axes(ax)
    ax.legend(frameon=False)
    fig.tight_layout()
    fig.savefig(out_dir / "efficiency.png", dpi=200)
    plt.close(fig)


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: plot_results.py <graph_name> [bench_dir=results/bench/<graph_name>] [out_dir=results/plots/<graph_name>]")
    gname = sys.argv[1]
    bench_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("results/bench") / gname
    out_dir = Path(sys.argv[3]) if len(sys.argv) > 3 else Path("results/plots") / gname
    out_dir.mkdir(parents=True, exist_ok=True)

    dijkstra_stats = load_dijkstra(bench_dir)
    dsa_stats = load_dsa(bench_dir)

    print_table(gname, dijkstra_stats, dsa_stats)
    write_summary_csv(out_dir / "summary.csv", gname, dijkstra_stats, dsa_stats)

    plot_time(gname, dsa_stats, out_dir)
    plot_speedup(gname, dijkstra_stats, dsa_stats, out_dir)
    plot_efficiency(gname, dijkstra_stats, dsa_stats, out_dir)

    print(f"\nwrote plots + summary to {out_dir}/")


if __name__ == "__main__":
    main()
