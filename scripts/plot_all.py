#!/usr/bin/env python3
#
# plot_all.py - one consolidated speedup figure, small multiples (one
# panel per graph), each showing the Delta-stepping and Wasp speedup
# curves against both the ideal-linear-speedup diagonal and the Dijkstra
# (T1) breakeven line. Reads the same results/bench/<graph>/ CSVs as
# plot_results.py.
#
# Usage:
#   python3 scripts/plot_all.py [graph_name ...] [--out results/plots/all_speedup.png]

import csv
import statistics
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

DEFAULT_GRAPHS = ["cit-Patents", "soc-pokec-relationships", "soc-LiveJournal1", "com-orkut.ungraph"]

COLOR_DSA = "#2a78d6"
COLOR_WASP = "#eb6834"
COLOR_IDEAL = "#898781"
COLOR_BASELINE = "#52514e"
COLOR_INK = "#0b0b0b"
COLOR_GRID = "#e1e0d9"


def read_times(path):
    times = []
    with open(path) as f:
        for row in csv.DictReader(f):
            times.append(float(row["time_ms"]))
    return times


def mean_time(path):
    return statistics.mean(read_times(path))


def load_series(bench_dir, algo, t1):
    per_thread = {}
    for path in sorted(bench_dir.glob(f"{algo}_t*.csv")):
        threads = int(path.stem.rsplit("_t", 1)[1])
        per_thread[threads] = mean_time(path)
    if not per_thread:
        sys.exit(f"no {algo}_t*.csv files found in {bench_dir}")

    threads = sorted(per_thread)
    speedup = [t1 / per_thread[t] for t in threads]
    return threads, speedup


def load_graph(gname, bench_root):
    bench_dir = bench_root / gname
    dij_path = bench_dir / "dijkstra.csv"
    if not dij_path.exists():
        sys.exit(f"missing {dij_path}")
    t1 = mean_time(dij_path)

    dsa_threads, dsa_speedup = load_series(bench_dir, "dsa", t1)
    wasp_threads, wasp_speedup = load_series(bench_dir, "wasp", t1)
    return dsa_threads, dsa_speedup, wasp_threads, wasp_speedup


def style_axes(ax):
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_color(COLOR_GRID)
    ax.spines["bottom"].set_color(COLOR_GRID)
    ax.grid(True, color=COLOR_GRID, linewidth=0.8, zorder=0)
    ax.tick_params(colors=COLOR_INK, labelsize=8)


def plot_panel(ax, gname, dsa_threads, dsa_speedup, wasp_threads, wasp_speedup):
    ax.plot(dsa_threads, dsa_threads, color=COLOR_IDEAL, linestyle="--", linewidth=1.3, label="ideal speedup")
    ax.axhline(1.0, color=COLOR_BASELINE, linestyle=":", linewidth=1.3, label="Dijkstra (T1) baseline")
    ax.plot(dsa_threads, dsa_speedup, color=COLOR_DSA, marker="o", markersize=5,
             linewidth=1.8, label="Delta-stepping")
    ax.plot(wasp_threads, wasp_speedup, color=COLOR_WASP, marker="o", markersize=5,
             linewidth=1.8, label="Wasp")

    ax.set_xscale("log", base=2)
    ax.set_yscale("log", base=2)
    ax.set_xticks(dsa_threads)
    ax.set_xticklabels(dsa_threads)
    ax.set_title(gname, fontsize=9, color=COLOR_INK)
    ax.set_xlabel("threads", fontsize=8)
    ax.set_ylabel("speedup vs. T1 (×)", fontsize=8)
    style_axes(ax)


def main():
    args = sys.argv[1:]
    out_path = Path("results/plots/all_speedup.png")
    if "--out" in args:
        idx = args.index("--out")
        out_path = Path(args[idx + 1])
        del args[idx:idx + 2]
    graphs = args if args else DEFAULT_GRAPHS

    bench_root = Path("results/bench")
    out_path.parent.mkdir(parents=True, exist_ok=True)

    n = len(graphs)
    ncols = 2
    nrows = (n + ncols - 1) // ncols
    fig, axes = plt.subplots(nrows, ncols, figsize=(8, 3.2 * nrows))
    axes = axes.flatten() if n > 1 else [axes]

    for i, gname in enumerate(graphs):
        dsa_threads, dsa_speedup, wasp_threads, wasp_speedup = load_graph(gname, bench_root)
        plot_panel(axes[i], gname, dsa_threads, dsa_speedup, wasp_threads, wasp_speedup)
        print(f"{gname}: dsa {dsa_speedup[0]:.2f}x (t={dsa_threads[0]}) -> {dsa_speedup[-1]:.2f}x (t={dsa_threads[-1]}), "
              f"wasp {wasp_speedup[0]:.2f}x (t={wasp_threads[0]}) -> {wasp_speedup[-1]:.2f}x (t={wasp_threads[-1]})")

    for j in range(n, len(axes)):
        axes[j].axis("off")

    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", ncol=4, frameon=False, fontsize=9,
               bbox_to_anchor=(0.5, -0.02))
    fig.tight_layout(rect=(0, 0.04, 1, 1))
    fig.savefig(out_path, dpi=200)
    print(f"\nwrote {out_path}")


if __name__ == "__main__":
    main()
