# src file documentation
## main.c

The main file execute, based on the given parameter, the different type of SpMV implemented in the `utils.h` file measuring the elapsed time. Two main mode of printing result are given using the parameter `print`:
- with `print` the result is in human-readable format, giving info such as matrix name, dimension and execution info. An example is:
```
./exec mtx/Fashion_MNIST_small.mtx 8 static 100 print
File: Fashion_MNIST_small.mtx
Rows: 10000
Cols: 10000
NNZ: 79152
Threads: 8
Type: static
Chunk: 100
Time: 0.003888698 s
```
- without `print` the reusult is in a comma-separated format, make to integrate the log file creation in the pbs script. An example is:
```
./exec mtx/Fashion_MNIST_small.mtx 8 static 100
8,static,100,0.004723598
```
## utils.h

In the `utils.h` file are defined usefull struct and function for the project

#### `typedef struct CSR`
Represents a sparse matrix in Compressed Sparse Row (CSR) format, storing row offsets, column indices, non-zero values.

#### `CSR csr_from_mtx(const char *filename)`
Parses a Matrix Market (`.mtx`) file and converts it into an internal CSR data structure, returning a fully initialized `CSR` object

#### `float* generate_vector(size_t n)`
Allocates and returns a vector of length `n`, filled with random floating-point values in the range `[0,1]`

`void csr_serial_spmv(CSR *m, const float *v, float *r)`
Computes the sparse matrix-vector mutliplication `r = m * v` using a serial CSR implementation

`void csr_omp_static(CSR *m, const float *v, float *r, int num_threads, int chunk_size)`

`void csr_omp_dynamic(CSR *m, const float *v, float *r, int num_threads, int chunk_size)`

`void csr_omp_guided(CSR *m, const float *v, float *r, int num_threads, int chunk_size)`

`bool compare_vectors(const float *a, const float *b, size_t n)`

