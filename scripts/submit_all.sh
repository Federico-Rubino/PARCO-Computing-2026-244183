#!/usr/bin/env bash
#
# submit_all.sh - submit one PBS job per graph, so the scheduler can run
# them concurrently instead of one long sequential sweep.
#
# Usage:
#   ./scripts/submit_all.sh
#

set -euo pipefail

# graph_file  undirected  delta -- edit once the graphs are downloaded
GRAPHS=(
    "data/roadNet-CA.txt        1  10"
    "data/web-Google.txt        0  10"
    "data/facebook_combined.txt 1  10"
    "data/cit-Patents.txt       0  10"
)

for entry in "${GRAPHS[@]}"; do
    read -r graph undirected delta <<< "$entry"
    gname="$(basename "$graph" .txt)"
    qsub -N "sssp_${gname}" -v GRAPH="$graph",UNDIRECTED="$undirected",DELTA="$delta" \
        scripts/run_experiments.pbs
done
