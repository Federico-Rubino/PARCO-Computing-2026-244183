# OpenMP Scheduling strategies for CSR-Based SpMV
## Index
- [What's in the repo](#What's-in-the-repo)
- [How to compile and run the code](#How-to-compile-and-run-the-code)
- [Log file format](#Log-file-format)

## What's in the repo
### src folder
In src folder there are the `main.c` file that need to be compiled (see [compile and run](#How-to-compile-and-run-the-code) section) and the `utils.h` files that provides the function used in the main, explained in the README.md file of the folder.

## How to compile and run the code
### On PBS-managed cluster
When running the code on a PBS-managed cluster, the provided PBS script (in the pbs folder is an example with explanation) can be used. The script executes the program multiple times with different parameter combinations and writes a log file that is useful for benchmarking data analysis.
### On personal PC
Firstly compile the code using `gcc -g -Wall -fopenmp src/main.c -o exec`.
To run the executable one single time with detailed output use the parameter `print` such as in the example:
```
gcc -g -Wall -fopenmp src/main.c -o exec
./exec filename.mtx 10 static 100 print
```

## Log file format
When running the code on a PBS-managed cluster, the provided PBS script can be used. The script executes the program multiple times with different parameter combinations and writes a log file that is useful for benchmarking data analysis. The output file is in CSV format to ensure portability and easy integration with libraries such as Python's Pandas for further analisys.

The CSV file begin with three commented header lines (prefixed with # so remember to use `pd.read_csv('filename.csv', comment="#")` in Pandas) that provide useful information about the simulations:
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
