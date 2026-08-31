#!/usr/bin/env bash
#
# submit_cache.sh - submit one PBS job per graph x algo (dsa, wasp) for the
# main cache-miss sweep (profile_cache.sh), so they can run concurrently
# instead of one long job per graph. com-orkut (the biggest graph) is
# further split by thread count -- even a single algo's 4-thread-count
# sweep didn't finish there in 5h.
#
# Usage:
#   ./scripts/submit_cache.sh
#

set -euo pipefail

mkdir -p logs

# graph_file  undirected  delta -- same tuned values used everywhere else
GRAPHS=(
    "data/cit-Patents.txt                0  50"
    "data/soc-pokec-relationships.txt    0  5"
    "data/soc-LiveJournal1.txt           0  5"
)

ALGOS=(dsa wasp)

for entry in "${GRAPHS[@]}"; do
    read -r graph undirected delta <<< "$entry"
    gname="$(basename "$graph" .txt)"
    for algo in "${ALGOS[@]}"; do
        qsub -N "cache_${gname}_${algo}" -o "logs/cache_${gname}_${algo}.log" -e "logs/cache_${gname}_${algo}.err" \
            -v GRAPH="$graph",UNDIRECTED="$undirected",DELTA="$delta",ALGO="$algo" \
            scripts/profile_cache.pbs
    done
done

# com-orkut: one job per algo x thread-half
COM_ORKUT="data/com-orkut.ungraph.txt"
COM_ORKUT_DELTA=2
THREAD_HALVES=(1_4 16_64)

for algo in "${ALGOS[@]}"; do
    for half in "${THREAD_HALVES[@]}"; do
        qsub -N "cache_com-orkut_${algo}_${half}" -o "logs/cache_com-orkut_${algo}_${half}.log" -e "logs/cache_com-orkut_${algo}_${half}.err" \
            -v GRAPH="$COM_ORKUT",UNDIRECTED=1,DELTA="$COM_ORKUT_DELTA",ALGO="$algo",THREADS="$half" \
            scripts/profile_cache.pbs
    done
done
