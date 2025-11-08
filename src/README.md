# src file documentation
## main.c

## utils.h

In the `utils.h` file are defined usefull struct and function for the project

### `typedef struct CSR`

### `CSR csr_from_mtx(const char *filename)`

### `float* generate_vector(size_t n)`

### `void csr_serial_spmv(CSR *m, const float *v, float *r)`

### `void csr_omp_static(CSR *m, const float *v, float *r, int num_threads, int chunk_size)`

### `void csr_omp_dynamic(CSR *m, const float *v, float *r, int num_threads, int chunk_size)`

### `void csr_omp_guided(CSR *m, const float *v, float *r, int num_threads, int chunk_size)`

### `bool compare_vectors(const float *a, const float *b, size_t n)`

### ``
