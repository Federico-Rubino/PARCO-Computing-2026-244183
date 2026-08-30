#!/usr/bin/env bash
#
# profile_cache.sh - cache-miss profiling via Valgrind/Cachegrind, run
# separately from the timing sweep. Cachegrind fully instruments every
# memory access (much slower than a real run) but is deterministic, so
# one run per configuration is enough - no warmup, no repetition.
#
# Usage:
#   ./scripts/profile_cache.sh [graph_file] [undirected=0] [delta=10.0] [schedule=dynamic,256]
#

set -euo pipefail

GRAPH="${1:-data/web-Google.txt}"
UNDIRECTED="${2:-0}"
DELTA="${3:-10.0}"
SCHEDULE="${4:-dynamic,256}"
THREADS=(1 4 16 64)

GNAME="$(basename "$GRAPH" .txt)"
OUT_DIR="results/cache"

mkdir -p "$OUT_DIR"

export OMP_SCHEDULE="$SCHEDULE"

command -v valgrind >/dev/null 2>&1 || { echo "valgrind not found in PATH" >&2; exit 1; }

echo "== Dijkstra cache profile: $GNAME =="
valgrind --tool=cachegrind --cache-sim=yes \
    --cachegrind-out-file="$OUT_DIR/${GNAME}_dijkstra.out" \
    ./bin/dijkstra_prof "$GRAPH" "$UNDIRECTED" auto 0 1 \
    > /dev/null 2> "$OUT_DIR/${GNAME}_dijkstra.log"

echo "== Delta-stepping cache profile: $GNAME (delta=$DELTA) =="
for T in "${THREADS[@]}"; do
    export OMP_NUM_THREADS=$T
    echo "-> threads: $T"
    valgrind --tool=cachegrind --cache-sim=yes \
        --cachegrind-out-file="$OUT_DIR/${GNAME}_dsa_t${T}.out" \
        ./bin/dsa_prof "$GRAPH" "$UNDIRECTED" auto "$DELTA" 0 1 \
        > /dev/null 2> "$OUT_DIR/${GNAME}_dsa_t${T}.log"
done

echo "== done: logs in $OUT_DIR/ =="
