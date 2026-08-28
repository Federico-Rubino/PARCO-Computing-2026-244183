#!/usr/bin/env bash
#
# run_experiments.sh - strong scaling sweep for Delta-Stepping, verified
# against the Dijkstra ground truth.
#
# Usage:
#   ./scripts/run_experiments.sh [graph_file] [undirected=0] [delta=10.0]
#

set -euo pipefail

GRAPH="${1:-data/web-Google.txt}"
UNDIRECTED="${2:-0}"
DELTA="${3:-10.0}"
THREADS=(1 2 4 8 16 32 64)
WARMUP_RUNS=5
NUM_RUNS=10

GNAME="$(basename "$GRAPH" .txt)"
GT_DIST="results/ground_truth/${GNAME}_dist.txt"

mkdir -p results/bench results/ground_truth

export OMP_PLACES=cores
export OMP_PROC_BIND=close
export OMP_SCHEDULE="dynamic,64"

if [ ! -f "$GT_DIST" ]; then
    echo "== Dijkstra ground truth: $GNAME =="
    ./bin/dijkstra "$GRAPH" "$UNDIRECTED" 0 "$WARMUP_RUNS" "$NUM_RUNS" \
        "results/bench/${GNAME}_dijkstra.csv" "$GT_DIST"
fi

echo "== Delta-stepping strong scaling: $GNAME (delta=$DELTA) =="

for T in "${THREADS[@]}"; do
    export OMP_NUM_THREADS=$T

    CSV_OUT="results/bench/${GNAME}_dsa_t${T}.csv"
    DIST_OUT="results/bench/${GNAME}_dsa_t${T}_dist.txt"

    echo "-> threads: $T"
    ./bin/dsa "$GRAPH" "$UNDIRECTED" 0 "$DELTA" "$WARMUP_RUNS" "$NUM_RUNS" \
        "$CSV_OUT" "$DIST_OUT"

    python3 scripts/compare_dist.py --quiet "$GT_DIST" "$DIST_OUT"
    rm -f "$DIST_OUT"
done

echo "== done: results in results/bench/ =="
