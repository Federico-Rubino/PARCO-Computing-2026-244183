#!/usr/bin/env python3
import matplotlib.pyplot as plt
import numpy as np
import glob
import argparse
import sys
import os
import csv


# Load CSV files
def load_csv_folder(folder):
    files = glob.glob(os.path.join(folder, "*.csv"))
    if not files:
        raise RuntimeError(f"No CSV files found in folder: {folder}")

    datasets = []

    for f in files:
        with open(f, "r", encoding="utf-8") as fh:
            reader = list(csv.reader(fh))

        # Parse header "# matrix rows cols nnz"
        header = reader[0][0].replace("#", "").strip()
        parts = header.split()

        matrix_name = parts[0]
        nnz = int(parts[-1])
        gflop_base = (2.0 * nnz) / 1e9  # GFLOPs

        serial_time = None
        static = {}
        nnz_bal = {}

        for row in reader[1:]:
            if row[0].startswith("#"):
                continue

            if len(row) != 3:
                continue

            threads = int(row[0])
            sched = row[1].strip()
            elapsed = float(row[2])

            if sched == "serial":
                serial_time = elapsed
            elif sched == "static":
                static[threads] = gflop_base / elapsed
            elif sched == "nnzbal":
                nnz_bal[threads] = gflop_base / elapsed

        if serial_time is None:
            raise ValueError(f"No serial entry found in {f}")

        serial_perf = gflop_base / serial_time

        datasets.append({
            "matrix": matrix_name,
            "serial": serial_perf,
            "static": static,
            "nnz_bal": nnz_bal,
        })

    return datasets


def main():
    parser = argparse.ArgumentParser(
        description="Plot GFLOP/s for multiple matrices from CSV files."
    )
    parser.add_argument("--input", required=True, help="Input folder containing CSV files")
    parser.add_argument("--output", required=True, help="Output folder for the plot")
    parser.add_argument("--threads", type=int, default=64,
                        help="Thread count used to extract GFLOP/s (default=64)")

    args = parser.parse_args()

    input_folder = args.input
    output_folder = args.output
    THREAD_REF = args.threads

    if not os.path.isdir(input_folder):
        print(f"Error: input folder '{input_folder}' does not exist.")
        sys.exit(1)

    os.makedirs(output_folder, exist_ok=True)

    datasets = load_csv_folder(input_folder)

    matrix_names = []
    serial_perf = []
    static_perf = []
    nnzbal_perf = []

    for data in datasets:
        matrix_names.append(data["matrix"])

        serial_perf.append(data["serial"])

        static_perf.append(
            data["static"].get(THREAD_REF, 0)
        )


        nnzbal_perf.append(
            data["nnz_bal"].get(THREAD_REF, 0)
        )

    
    color_serial = "#0072B2"   # blue
    color_static = "#E69F00"   # orange
    color_nnzbal = "#009E73"   # green

    plt.figure(figsize=(7.0, 3.5))

    x = np.arange(len(matrix_names))
    width = 0.25

    plt.bar(
        x - width, serial_perf, width,
        label="Serial",
        color=color_serial, edgecolor="black", hatch="///"
    )

    plt.bar(
        x, static_perf, width,
        label="Static Scheduling",
        color=color_static, edgecolor="black", hatch="\\\\\\"
    )

    plt.bar(
        x + width, nnzbal_perf, width,
        label="NNZ-Balanced Scheduling",
        color=color_nnzbal, edgecolor="black", hatch="xx"
    )

    plt.xticks(x, matrix_names, rotation=25, fontsize=9)
    plt.yticks(fontsize=10)

    plt.xlabel("Matrix", fontsize=11, fontweight="bold")
    plt.ylabel(f"GFLOP/s - {THREAD_REF} Threads",
               fontsize=11, fontweight="bold")
    plt.title("SpMV GFLOP/s Comparison Across Matrices",
              fontsize=12, fontweight="bold")

    plt.grid(axis="y", alpha=0.35, linestyle="--", linewidth=0.5)
    plt.legend(fontsize=9, frameon=False)

    plt.tight_layout()

    outfile = os.path.join(output_folder, "spmv_gflops_comparison.png")
    plt.savefig(outfile, dpi=300)

    print(f"Plot saved as: {outfile}")
    plt.show()


if __name__ == "__main__":
    main()
