#!/usr/bin/env bash
#
# submit_all.sh - submit one PBS job per graph, so the scheduler can run
# them concurrently instead of one long sequential sweep.
#
# Usage:
#   ./scripts/submit_all.sh
#

set -euo pipefail

mkdir -p logs

# graph_file  undirected  delta  schedule (comma replaced by underscore,
# qsub -v splits on comma) -- delta/schedule below are still placeholders
# for graphs not yet tuned; only soc-LiveJournal1 has been tuned so far
# (see results/tuning/), via tune_params.sh + verify_tuning_grid.sh
GRAPHS=(
    "data/roadNet-CA.txt        1  10  dynamic_64"
    "data/web-Google.txt        0  10  dynamic_64"
    "data/facebook_combined.txt 1  10  dynamic_64"
    "data/cit-Patents.txt       0  10  dynamic_64"
    "data/soc-LiveJournal1.txt  0  5   dynamic_256"
)

for entry in "${GRAPHS[@]}"; do
    read -r graph undirected delta schedule <<< "$entry"
    gname="$(basename "$graph" .txt)"
    qsub -N "sssp_${gname}" -o "logs/${gname}.log" -e "logs/${gname}.err" \
        -v GRAPH="$graph",UNDIRECTED="$undirected",DELTA="$delta",SCHEDULE="$schedule" \
        scripts/run_experiments.pbs
done
