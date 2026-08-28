#!/usr/bin/env bash
#
# verify_tuning_grid.sh - small cross-product check of the top Delta and
# schedule candidates from tune_params.sh, to confirm they don't interact
# strongly. Justifies sequential (not full grid) tuning, and reusing one
# schedule across graphs. Run after tune_params.sh has already produced
# its sweep for this graph (needs the cached Dijkstra ground truth).
#
# Usage:
#   ./scripts/verify_tuning_grid.sh [graph_file] [undirected=0] [threads=32]
#

set -euo pipefail

GRAPH="${1:-data/web-Google.txt}"
UNDIRECTED="${2:-0}"
THREADS="${3:-32}"
DELTAS=(1 2 5)
SCHEDULES=("dynamic,64" "dynamic,256")
WARMUP_RUNS=5
NUM_RUNS=10

GNAME="$(basename "$GRAPH" .txt)"
GT_DIST="results/ground_truth/${GNAME}_dist.txt"
OUT_DIR="results/tuning/grid"

mkdir -p "$OUT_DIR"

if [ ! -f "$GT_DIST" ]; then
    echo "missing $GT_DIST -- run tune_params.sh or run_experiments.sh on this graph first" >&2
    exit 1
fi

export OMP_PLACES=cores
export OMP_PROC_BIND=close
export OMP_NUM_THREADS=$THREADS

echo "== verification grid: $GNAME (threads=$THREADS) =="

for D in "${DELTAS[@]}"; do
    for S in "${SCHEDULES[@]}"; do
        export OMP_SCHEDULE="$S"
        TAG="${S//,/_}"
        CSV_OUT="$OUT_DIR/${GNAME}_delta${D}_sched${TAG}.csv"
        DIST_OUT="$OUT_DIR/${GNAME}_delta${D}_sched${TAG}_dist.txt"

        echo "-> delta=$D schedule=$S"
        ./bin/dsa "$GRAPH" "$UNDIRECTED" 0 "$D" "$WARMUP_RUNS" "$NUM_RUNS" \
            "$CSV_OUT" "$DIST_OUT"

        python3 scripts/compare_dist.py --quiet "$GT_DIST" "$DIST_OUT"
        rm -f "$DIST_OUT"
    done
done

python3 scripts/analyze_tuning.py "$OUT_DIR" "$GNAME" grid

echo "== done: full results in $OUT_DIR/ =="
