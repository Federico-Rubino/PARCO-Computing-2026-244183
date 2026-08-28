#!/usr/bin/env bash
#
# tune_params.sh - sweep Delta, then OMP_SCHEDULE, at one fixed thread
# count, to pick a configuration for the main strong-scaling sweep. Run
# once per graph before run_experiments.sh.
#
# Usage:
#   ./scripts/tune_params.sh [graph_file] [undirected=0] [threads=32]
#

set -euo pipefail

GRAPH="${1:-data/web-Google.txt}"
UNDIRECTED="${2:-0}"
THREADS="${3:-32}"
DELTAS=(1 2 5 10 20 50)
SCHEDULES=("static" "dynamic,16" "dynamic,64" "dynamic,256" "guided")
WARMUP_RUNS=5
NUM_RUNS=10

GNAME="$(basename "$GRAPH" .txt)"
GT_DIST="results/ground_truth/${GNAME}_dist.txt"
OUT_DIR="results/tuning"

mkdir -p "$OUT_DIR" results/bench results/ground_truth

export OMP_PLACES=cores
export OMP_PROC_BIND=close
export OMP_NUM_THREADS=$THREADS

if [ ! -f "$GT_DIST" ]; then
    echo "== Dijkstra ground truth: $GNAME =="
    ./bin/dijkstra "$GRAPH" "$UNDIRECTED" 0 "$WARMUP_RUNS" "$NUM_RUNS" \
        "results/bench/${GNAME}_dijkstra.csv" "$GT_DIST"
fi

echo "== delta sweep: $GNAME (threads=$THREADS, schedule=dynamic,64) =="
export OMP_SCHEDULE="dynamic,64"

for D in "${DELTAS[@]}"; do
    CSV_OUT="$OUT_DIR/${GNAME}_delta${D}.csv"
    DIST_OUT="$OUT_DIR/${GNAME}_delta${D}_dist.txt"

    echo "-> delta: $D"
    ./bin/dsa "$GRAPH" "$UNDIRECTED" 0 "$D" "$WARMUP_RUNS" "$NUM_RUNS" \
        "$CSV_OUT" "$DIST_OUT"

    python3 scripts/compare_dist.py --quiet "$GT_DIST" "$DIST_OUT"
    rm -f "$DIST_OUT"
done

BEST_DELTA="$(python3 scripts/analyze_tuning.py "$OUT_DIR" "$GNAME" delta)"
echo "== best delta: $BEST_DELTA =="

echo "== schedule sweep: $GNAME (threads=$THREADS, delta=$BEST_DELTA) =="

for S in "${SCHEDULES[@]}"; do
    export OMP_SCHEDULE="$S"
    TAG="${S//,/_}"
    CSV_OUT="$OUT_DIR/${GNAME}_sched${TAG}.csv"
    DIST_OUT="$OUT_DIR/${GNAME}_sched${TAG}_dist.txt"

    echo "-> schedule: $S"
    ./bin/dsa "$GRAPH" "$UNDIRECTED" 0 "$BEST_DELTA" "$WARMUP_RUNS" "$NUM_RUNS" \
        "$CSV_OUT" "$DIST_OUT"

    python3 scripts/compare_dist.py --quiet "$GT_DIST" "$DIST_OUT"
    rm -f "$DIST_OUT"
done

python3 scripts/analyze_tuning.py "$OUT_DIR" "$GNAME"

echo "== done: full results in $OUT_DIR/ =="
