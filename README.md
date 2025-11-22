# OpenMP Scheduling strategies for CSR-Based SpMV
## Index
- [What's in the repo](#What's-in-the-repo)
- [How to compile and run the code](#How-to-compile-and-run-the-code)
- [Log file format](#Log-file-format)

## What's in the repo
### src folder
In src folder there are the `main.c` file that need to be compiled (see [compile and run](#How-to-compile-and-run-the-code) section) and the `utils.h` file that provides the function used in the main, explained in the README.md file of the folder.

### plots folder
Plots folder contain different type of plots of the data from experiments runned on matrix in mtx.csv file

### results folder
Results folder contain csv log file from the experiments runned on matrix in mtx.csv file

### scripts folder
Scripts folder contain usefull python scripts to analyze csv from results folder (and generating plot), and the pbs script to run the experiment on a PBS managed cluster. Note that the pbs script need to be modified with correct directories as explained in the README.md file of the folder.

### mtx.csv
This file contain the link to the matrix zip from SuiteSparse

## How to compile and run the code
### On PBS-managed cluster
When running the code on a PBS-managed cluster, the provided PBS script can be used (in the scripts folder there is an example with explanation). The script executes the program multiple times with different parameter combinations and writes a log file that is useful for benchmarking data analysis.
### On personal PC
Firstly compile the code using `gcc -fopenmp -march=native -O3 src/main.c -o exec`.
The executable need some arguments: 
```
Usage: ./exec <file.mtx> <num_threads> <type>
Types: serial | static | nnzbal
```

## Log file format
When running the code on a PBS-managed cluster, the provided PBS script can be used. The script executes the program multiple times with different parameter combinations and writes a log file that is useful for benchmarking data analysis. The output file is in CSV format to ensure portability and easy integration for further analisys.

The CSV file begin with one commented header lines (prefixed with #) that provide useful information about the the matrix on which the simulation is runned:
```
# matrix_name rows columns nnz_values
```
This is followed by the actual data, structured as:
```csv
<n_thread>,<type>,<time_taken>
.
.
.
<n_thread>,<type>,<time_taken>
```
