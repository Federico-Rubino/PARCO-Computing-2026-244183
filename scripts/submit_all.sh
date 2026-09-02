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

# graph_file  undirected  delta  schedule -- tuned via tune_params.sh + verify_tuning_grid.sh
GRAPHS=(
    "data/cit-Patents.txt                0  50  dynamic_256"
    "data/soc-pokec-relationships.txt    0  5   dynamic_256"
    "data/soc-LiveJournal1.txt           0  5   dynamic_256"
    "data/com-orkut.ungraph.txt          1  2   dynamic_256"
)

for entry in "${GRAPHS[@]}"; do
    read -r graph undirected delta schedule <<< "$entry"
    gname="$(basename "$graph" .txt)"
    qsub -N "sssp_${gname}" -o "logs/${gname}.log" -e "logs/${gname}.err" \
        -v GRAPH="$graph",UNDIRECTED="$undirected",DELTA="$delta",SCHEDULE="$schedule" \
        scripts/run_experiments.pbs
done
