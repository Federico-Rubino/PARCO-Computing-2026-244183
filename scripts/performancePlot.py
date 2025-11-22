#!/usr/bin/env python3
import matplotlib.pyplot as plt
import numpy as np
import csv
import argparse
import sys
import os

def main():
    parser = argparse.ArgumentParser(description="Plot SpMV performance from CSV")
    parser.add_argument("--csv", required=True, help="Input CSV file")
    parser.add_argument("--outdir", required=True, help="Output folder for saving the plot")
    args = parser.parse_args()

    csv_file = args.csv
    outdir = args.outdir

    if not os.path.isfile(csv_file):
        print(f"Error: file '{csv_file}' not found.")
        sys.exit(1)

    # Create output folder if it does not exist
    if not os.path.isdir(outdir):
        print(f"Output directory '{outdir}' does not exist. Creating it...")
        os.makedirs(outdir, exist_ok=True)

    # Read CSV file
    with open(csv_file, "r") as f:
        reader = list(csv.reader(f))

    # Parse header: "# matrix rows cols nnz"
    header = reader[0][0]
    header_parts = header.split()
    matrix_name = header_parts[1]
    nnz = int(header_parts[-1])
    gflop = (2.0 * nnz) / 1e9

    # Parse performance data
    serial_time = None
    static = {}
    nnz_bal = {}

    for row in reader[1:]:
        if row[0].startswith("#"):
            continue
        threads = int(row[0])
        sched = row[1].strip()
        elapsed = float(row[2])
        if sched == "serial":
            serial_time = elapsed
        elif sched == "static":
            static[threads] = gflop / elapsed
        elif sched == "nnzbal":
            nnz_bal[threads] = gflop / elapsed

    thread_values = [1, 2, 4, 8, 16, 32, 64]

    serial_perf = [gflop / serial_time] * len(thread_values)
    static_perf = [static[t] for t in thread_values]
    nnz_perf   = [nnz_bal[t] for t in thread_values]


    color_serial = "#0072B2"   # blue
    color_static = "#E69F00"   # orange
    color_nnzbal = "#009E73"   # green

    # Plot Bar Chart
    plt.figure(figsize=(6.0, 3.2))
    x = np.arange(len(thread_values))
    width = 0.25

    plt.bar(x - width, serial_perf, width,
            label="Serial",
            color=color_serial,
            edgecolor="black",
            hatch="///")
    plt.bar(x, static_perf, width,
            label="Static Scheduling",
            color=color_static,
            edgecolor="black",
            hatch="\\\\\\")
    plt.bar(x + width, nnz_perf, width,
            label="NNZ-Balanced Scheduling",
            color=color_nnzbal,
            edgecolor="black",
            hatch="xx")

    plt.xticks(x, thread_values, fontsize=10)
    plt.yticks(fontsize=10)
    plt.xlabel("Number of Threads", fontsize=11, fontweight="bold")
    plt.ylabel("Performance (GFLOP/s)", fontsize=11, fontweight="bold")
    plt.title(f"SpMV Performance – Matrix: {matrix_name}",
              fontsize=12, fontweight="bold")
    plt.grid(axis="y", alpha=0.35, linestyle="--", linewidth=0.5)
    plt.legend(fontsize=9, frameon=False)
    plt.tight_layout()

    output_file = os.path.join(outdir, f"{matrix_name}_bar.png")
    plt.savefig(output_file, dpi=300)
    print(f"Saved bar chart as: {output_file}")
    plt.close()

    # Plot Line Chart (Curves)
    plt.figure(figsize=(6.0, 3.2))

    plt.plot(thread_values, serial_perf, marker='o', linestyle='-', color=color_serial, label="Serial")
    plt.plot(thread_values, static_perf, marker='s', linestyle='--', color=color_static, label="Static Scheduling")
    plt.plot(thread_values, nnz_perf, marker='^', linestyle='-.', color=color_nnzbal, label="NNZ-Balanced Scheduling")

    plt.xticks(thread_values, fontsize=10)
    plt.yticks(fontsize=10)
    plt.xlabel("Number of Threads", fontsize=11, fontweight="bold")
    plt.ylabel("Performance (GFLOP/s)", fontsize=11, fontweight="bold")
    plt.title(f"SpMV Performance Scaling – Matrix: {matrix_name}", fontsize=12, fontweight="bold")
    plt.grid(True, linestyle="--", alpha=0.35, linewidth=0.5)
    plt.legend(fontsize=9, frameon=False)
    plt.tight_layout()

    output_file_line = os.path.join(outdir, f"{matrix_name}_line.png")
    plt.savefig(output_file_line, dpi=300)
    print(f"Saved line chart as: {output_file_line}")
    plt.show()


if __name__ == "__main__":
    main()
