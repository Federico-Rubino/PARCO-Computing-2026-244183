#!/usr/bin/env bash
#
# tune_params.sh - sweep Delta at one fixed thread count, to pick a
# per-graph configuration for the main strong-scaling sweep. Schedule is
# held fixed at the value already chosen via the schedule sweep + the
# verification grid on soc-LiveJournal1 (dynamic,256) - see
# results/tuning/ and verify_tuning_grid.sh. Run once per graph before
# run_experiments.sh.
#
# Usage:
#   ./scripts/tune_params.sh [graph_file] [undirected=0] [threads=32] [schedule=dynamic,256]
#

set -euo pipefail

GRAPH="${1:-data/web-Google.txt}"
UNDIRECTED="${2:-0}"
THREADS="${3:-32}"
SCHEDULE="${4:-dynamic,256}"
DELTAS=(1 2 5 10 20 50)
WARMUP_RUNS=5
NUM_RUNS=10

GNAME="$(basename "$GRAPH" .txt)"
BENCH_DIR="results/bench/$GNAME"
GT_DIST="results/ground_truth/${GNAME}_dist.txt"
OUT_DIR="results/tuning"

mkdir -p "$OUT_DIR" "$BENCH_DIR" results/ground_truth

export OMP_PLACES=cores
export OMP_PROC_BIND=close
export OMP_NUM_THREADS=$THREADS
export OMP_SCHEDULE="$SCHEDULE"

if [ ! -f "$GT_DIST" ]; then
    echo "== Dijkstra ground truth: $GNAME =="
    ./bin/dijkstra "$GRAPH" "$UNDIRECTED" 0 "$WARMUP_RUNS" "$NUM_RUNS" \
        "$BENCH_DIR/dijkstra.csv" "$GT_DIST"
fi

echo "== delta sweep: $GNAME (threads=$THREADS, schedule=$SCHEDULE) =="

for D in "${DELTAS[@]}"; do
    CSV_OUT="$OUT_DIR/${GNAME}_delta${D}.csv"
    DIST_OUT="$OUT_DIR/${GNAME}_delta${D}_dist.txt"

    echo "-> delta: $D"
    ./bin/dsa "$GRAPH" "$UNDIRECTED" 0 "$D" "$WARMUP_RUNS" "$NUM_RUNS" \
        "$CSV_OUT" "$DIST_OUT"

    python3 scripts/compare_dist.py --quiet "$GT_DIST" "$DIST_OUT"
    rm -f "$DIST_OUT"
done

python3 scripts/analyze_tuning.py "$OUT_DIR" "$GNAME"

echo "== done: full results in $OUT_DIR/ =="
