#!/usr/bin/env bash
#
# profile_cache.sh - cache-miss profiling via Valgrind/Cachegrind, run
# separately from the timing sweep. Cachegrind fully instruments every
# memory access (much slower than a real run) but is deterministic, so
# one run per configuration is enough - no warmup, no repetition.
#
# algo picks which parallel implementation to profile (dsa, wasp, or both
# with "all"), so a large graph can be split across several smaller jobs
# instead of one long one -- Dijkstra always runs too (cheap, and needed
# as the single-threaded reference), but is skipped if already logged.
#
# Usage:
#   ./scripts/profile_cache.sh [graph_file] [undirected=0] [delta=10.0] [schedule=dynamic,256] [algo=all]
#

set -euo pipefail

GRAPH="${1:-data/web-Google.txt}"
UNDIRECTED="${2:-0}"
DELTA="${3:-10.0}"
SCHEDULE="${4:-dynamic,256}"
ALGO="${5:-all}"
THREADS=(1 4 16 64)

case "$ALGO" in
    all|dsa|wasp) ;;
    *) echo "algo must be one of: all, dsa, wasp (got '$ALGO')" >&2; exit 1 ;;
esac

GNAME="$(basename "$GRAPH" .txt)"
OUT_DIR="results/cache"

mkdir -p "$OUT_DIR"

export OMP_SCHEDULE="$SCHEDULE"

command -v valgrind >/dev/null 2>&1 || { echo "valgrind not found in PATH" >&2; exit 1; }

DIJKSTRA_LOG="$OUT_DIR/${GNAME}_dijkstra.log"
if [ -f "$DIJKSTRA_LOG" ]; then
    echo "== Dijkstra cache profile: $GNAME (skip, already have $DIJKSTRA_LOG) =="
else
    echo "== Dijkstra cache profile: $GNAME =="
    valgrind --tool=cachegrind --cache-sim=yes \
        --cachegrind-out-file="$OUT_DIR/${GNAME}_dijkstra.out" \
        ./bin/dijkstra_prof "$GRAPH" "$UNDIRECTED" auto 0 1 \
        > /dev/null 2> "$DIJKSTRA_LOG"
    rm -f "$OUT_DIR/${GNAME}_dijkstra.out"
fi

if [ "$ALGO" = "all" ] || [ "$ALGO" = "dsa" ]; then
    echo "== Delta-stepping cache profile: $GNAME (delta=$DELTA) =="
    for T in "${THREADS[@]}"; do
        export OMP_NUM_THREADS=$T
        LOG="$OUT_DIR/${GNAME}_dsa_t${T}.log"
        if [ -f "$LOG" ]; then
            echo "-> threads: $T (skip, already have $LOG)"
            continue
        fi
        echo "-> threads: $T"
        valgrind --tool=cachegrind --cache-sim=yes \
            --cachegrind-out-file="$OUT_DIR/${GNAME}_dsa_t${T}.out" \
            ./bin/dsa_prof "$GRAPH" "$UNDIRECTED" auto "$DELTA" 0 1 \
            > /dev/null 2> "$LOG"
        rm -f "$OUT_DIR/${GNAME}_dsa_t${T}.out"
    done
fi

if [ "$ALGO" = "all" ] || [ "$ALGO" = "wasp" ]; then
    echo "== Wasp cache profile: $GNAME (delta=$DELTA) =="
    for T in "${THREADS[@]}"; do
        export OMP_NUM_THREADS=$T
        LOG="$OUT_DIR/${GNAME}_wasp_t${T}.log"
        if [ -f "$LOG" ]; then
            echo "-> threads: $T (skip, already have $LOG)"
            continue
        fi
        echo "-> threads: $T"
        valgrind --tool=cachegrind --cache-sim=yes \
            --cachegrind-out-file="$OUT_DIR/${GNAME}_wasp_t${T}.out" \
            ./bin/wasp_prof "$GRAPH" "$UNDIRECTED" auto "$DELTA" 0 1 \
            > /dev/null 2> "$LOG"
        rm -f "$OUT_DIR/${GNAME}_wasp_t${T}.out"
    done
fi

echo "== done: logs in $OUT_DIR/ =="
