#!/usr/bin/env bash
#
# submit_rr_compare.sh - submit one PBS job per graph for the with/without
# Rabbit-reordering comparison (profile_rr_compare.sh), so they can run
# concurrently instead of one at a time.
#
# Usage:
#   ./scripts/submit_rr_compare.sh
#

set -euo pipefail

mkdir -p logs

# graph_file  undirected  delta -- same tuned values used everywhere else
GRAPHS=(
    "data/cit-Patents.txt                0  50"
    "data/soc-pokec-relationships.txt    0  5"
    "data/soc-LiveJournal1.txt           0  5"
    "data/com-orkut.ungraph.txt          1  2"
)

for entry in "${GRAPHS[@]}"; do
    read -r graph undirected delta <<< "$entry"
    gname="$(basename "$graph" .txt)"
    qsub -N "rr_${gname}" -o "logs/rr_${gname}.log" -e "logs/rr_${gname}.err" \
        -v GRAPH="$graph",UNDIRECTED="$undirected",DELTA="$delta" \
        scripts/profile_rr_compare.pbs
done
