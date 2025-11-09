# src file documentation
## main.c

The main file execute, based on the given parameter, the different type of SpMV implemented in the `utils.h` file measuring the elapsed time. Two main mode of printing result are given using the parameter `print`:
- with `print` the result is in human-readable format, giving info such as matrix name, dimension and execution info. An example is:
```
```
- without `print` the reusult is in a comma-separated format, make to integrate the log file creation in the pbs script. An example is:
```
```
## utils.h

In the `utils.h` file are defined usefull struct and function for the project

`typedef struct CSR`

`CSR csr_from_mtx(const char *filename)`

`float* generate_vector(size_t n)`

`void csr_serial_spmv(CSR *m, const float *v, float *r)`

`void csr_omp_static(CSR *m, const float *v, float *r, int num_threads, int chunk_size)`

`void csr_omp_dynamic(CSR *m, const float *v, float *r, int num_threads, int chunk_size)`

`void csr_omp_guided(CSR *m, const float *v, float *r, int num_threads, int chunk_size)`

`bool compare_vectors(const float *a, const float *b, size_t n)`

