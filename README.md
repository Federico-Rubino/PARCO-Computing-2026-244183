# OpenMP Scheduling strategies for CSR-Based SpMV
## Index
- [What's in the repo](#What's-in-the-repo)
- [How to run the code](#How-to-run-the-code)
- [Log file format](#Log-file-format)

## What's in the repo

## How to run the code

## Log file format
When running the code in a pbs the pbs script provided can be used. The script, while running the code multiple time with different parameter combination, write a log file usefull for benchmark and data analisys. The file is in csv format to make it portable and usable with libraries like python Panda's. The file header is composed by three commented line, with # that provides usefull information about the simulation:
'''
matrix name rows columns nnz_values
n_thread_start n_thread_end n_thread_step
n_chunks_start n_chunks_end n_chunks_factor
'''
