#!/usr/bin/env bash
#
# profile_rr_compare.sh - with/without Rabbit-reordering comparison, at one
# fixed thread count, per graph: both cache-miss rate (Cachegrind) and
# wall-clock time (a normal timed run). Separate from profile_cache.sh's
# main per-thread sweep, which always runs with RR enabled (the default
# used for the main results).
#
# Usage:
#   ./scripts/profile_rr_compare.sh [graph_file] [undirected=0] [delta=10.0] [schedule=dynamic,256] [threads=32]
#

set -euo pipefail

GRAPH="${1:-data/web-Google.txt}"
UNDIRECTED="${2:-0}"
DELTA="${3:-10.0}"
SCHEDULE="${4:-dynamic,256}"
THREADS="${5:-32}"
WARMUP_RUNS=5
NUM_RUNS=10

GNAME="$(basename "$GRAPH" .txt)"
OUT_DIR="results/rr_compare/$GNAME"

mkdir -p "$OUT_DIR"

command -v valgrind >/dev/null 2>&1 || { echo "valgrind not found in PATH" >&2; exit 1; }

export OMP_PLACES=cores
export OMP_PROC_BIND=close
export OMP_SCHEDULE="$SCHEDULE"
export OMP_NUM_THREADS=$THREADS

echo "== RR comparison: $GNAME (threads=$THREADS, delta=$DELTA) =="

for RR in 1 0; do
    TAG=$([ "$RR" = "1" ] && echo "rr" || echo "norr")
    export RABBIT_REORDER=$RR

    echo "-> dijkstra, RABBIT_REORDER=$RR (timing)"
    ./bin/dijkstra "$GRAPH" "$UNDIRECTED" auto "$WARMUP_RUNS" "$NUM_RUNS" \
        "$OUT_DIR/dijkstra_${TAG}.csv"

    echo "-> dsa t=$THREADS, RABBIT_REORDER=$RR (timing)"
    ./bin/dsa "$GRAPH" "$UNDIRECTED" auto "$DELTA" "$WARMUP_RUNS" "$NUM_RUNS" \
        "$OUT_DIR/dsa_t${THREADS}_${TAG}.csv"

    echo "-> dijkstra, RABBIT_REORDER=$RR (cache)"
    valgrind --tool=cachegrind --cache-sim=yes \
        --cachegrind-out-file="$OUT_DIR/dijkstra_${TAG}.out" \
        ./bin/dijkstra_prof "$GRAPH" "$UNDIRECTED" auto 0 1 \
        > /dev/null 2> "$OUT_DIR/dijkstra_${TAG}.log"

    echo "-> dsa t=$THREADS, RABBIT_REORDER=$RR (cache)"
    valgrind --tool=cachegrind --cache-sim=yes \
        --cachegrind-out-file="$OUT_DIR/dsa_t${THREADS}_${TAG}.out" \
        ./bin/dsa_prof "$GRAPH" "$UNDIRECTED" auto "$DELTA" 0 1 \
        > /dev/null 2> "$OUT_DIR/dsa_t${THREADS}_${TAG}.log"
done

echo "== done: results in $OUT_DIR/ =="
