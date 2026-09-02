# Bulk-Synchronous vs. Asynchronous Parallel SSSP

Course project (Introduction to Parallel Computing, University of
Trento) comparing three Single-Source Shortest Path implementations in
OpenMP on real-world SNAP graphs:

- **Dijkstra** — sequential, binary min-heap with lazy deletion. Ground
  truth and the $T_1$ baseline for speedup.
- **Δ-stepping** — bulk-synchronous, bucket-coarsened, barrier per
  bucket.
- **Wasp** — asynchronous, priority-aware work stealing, no barrier;
  a simplified variant of D'Antonio et al.'s Wasp.

All three run on graphs preprocessed with Rabbit reordering. Results
include strong-scaling speedup, cache-miss profiling (Cachegrind), and
a with/without Rabbit-reordering comparison.

## Repository structure

```
src/
  dijkstra.c          sequential Dijkstra
  delta_stepping.c     parallel Δ-stepping
  wasp.c               parallel work-stealing SSSP
  reorder_graph.c      
  common/
    graph_csr.c/.h       CSR graph loader (SNAP edge-list format)
    graph_rr.c/.h        Rabbit reordering
    utils.c/.h

scripts/
  download_graphs.sh          fetch SNAP graphs into data/
  run_experiments.sh/.pbs      strong-scaling sweep (Dijkstra+dsa+wasp)
  submit_all.sh                 submit run_experiments.pbs, one job/graph
  profile_cache.sh/.pbs        per-thread-count Cachegrind sweep
  submit_cache.sh               submit profile_cache.pbs, one job/graph/algo
  profile_rr_compare.sh/.pbs   with/without-RR timing + cache comparison
  submit_rr_compare.sh          submit profile_rr_compare.pbs, one job/graph
  tune_params.sh                 Δ hyperparameter sweep
  verify_tuning_grid.sh          Δ×schedule cross-check
  compare_dist.py                checks a run's output against ground truth
  plot_results.py                per-graph time/speedup/efficiency plots
  plot_all.py                     4-graph speedup small-multiples figure
  analyze_cache.py               parses Cachegrind logs into summary.csv
  analyze_rr_compare.py          parses RR-comparison logs into summary.csv

data/        SNAP graph files
results/     benchmark CSVs, Cachegrind logs, plots (tracked, except
             results/ground_truth/, which is regenerated locally)
bin/         built binaries (gitignored)
```

## Reproducibility

### Requirements

- GCC 11+ (needs OpenMP 5.1's `atomic compare capture`; tested with
  GCC 13.3.0)
- Valgrind (for Cachegrind cache-miss profiling)
- Python 3 with `matplotlib` for plotting (`pip install matplotlib`)
- PBS/`qsub` if running on the cluster

### 1. Build

On the PBS cluster, load a GCC module first (binaries are built once on
the login node, before submitting any job):
```bash
module load GCC/13.3.0
```

```bash
make                                          # dsa, wasp, dijkstra, reorder
make dsa_prof wasp_prof dijkstra_prof         # non-native builds, for Cachegrind
```

### 2. Get the graphs

```bash
./scripts/download_graphs.sh https://snap.stanford.edu/data/cit-Patents.txt.gz
./scripts/download_graphs.sh https://snap.stanford.edu/data/soc-pokec-relationships.txt.gz
./scripts/download_graphs.sh https://snap.stanford.edu/data/soc-LiveJournal1.txt.gz
./scripts/download_graphs.sh https://snap.stanford.edu/data/bigdata/communities/com-orkut.ungraph.txt.gz
```
Each drops a ready-to-use `data/<graph>.txt` edge list.

### 3. Run the experiments

On the cluster, submit one job per graph and per graph × algorithm for
cache profiling:
```bash
./scripts/submit_all.sh          # strong scaling: Dijkstra + dsa + wasp, 1-64 threads
./scripts/submit_cache.sh        # Cachegrind sweep: dsa + wasp, threads {1,4,16,64}
./scripts/submit_rr_compare.sh   # with/without Rabbit reordering, timing + cache
```

Without a PBS scheduler, run the same underlying scripts directly, e.g.:
```bash
./scripts/run_experiments.sh data/cit-Patents.txt 0 50 dynamic,256
./scripts/profile_cache.sh data/cit-Patents.txt 0 50 dynamic,256 all
./scripts/profile_rr_compare.sh data/cit-Patents.txt 0 50 dynamic,256 32
```
(Δ and `undirected` per graph: see the `GRAPHS` array in
`scripts/submit_all.sh`.) Every run checks its output distances
against the Dijkstra ground truth automatically; a mismatch aborts the
sweep.

### 4. Analyze and plot

```bash
python3 scripts/plot_results.py <graph_name>   # per-graph table + plots
python3 scripts/plot_all.py                     # 4-graph speedup figure
python3 scripts/analyze_cache.py                 # results/cache/summary.csv
python3 scripts/analyze_rr_compare.py             # results/rr_compare/summary.csv
```
