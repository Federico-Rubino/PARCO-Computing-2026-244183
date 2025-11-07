# OpenMP Scheduling strategies for CSR-Based SpMV
## Index
- [What's in the repo](#What's-in-the-repo)
- [How to run the code](#How-to-run-the-code)
- [Log file format](#Log-file-format)

## What's in the repo

## How to run the code

## Log file format
When running the code on a PBS-managed cluster, the provided PBS script can be used. The script executes the program multiple times with different parameter combinations and writes a log file that is useful for benchmarking data analysis. The output file is in CSV format to ensure portability and easy integration with libraries such as Python's Pandas.

The CSV file begin with three commente header lines (prefixed with # so remember to use pd.read_csv('log.csv', comment="#") in Pandas) that provide useful information about the simulations:
```
# matrix_name rows columns nnz_values
# n_thread_start n_thread_end n_thread_step
# n_chunks_start n_chunks_end n_chunks_factor
```
This is followed by the actual data, structured as:
```
n_thread, schedule_type, chunks, elapsed_time
<n_thread>,<type>,<chunks - null if serial>,<time_taken>
.
.
.
<n_thread>,<type>,<chunks - null if serial>,<time_taken>
```
