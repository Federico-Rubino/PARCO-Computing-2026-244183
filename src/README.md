# src file documentation
## main.c

The `main.c` file execute, based on the given parameter, the different type of SpMV implemented in the `utils.h` file measuring the 90th percentile of elapsed time after cache warm-up and 10 iteration with the same parameters. The program takes parameter as argument:
```
Usage: ./exec <file.mtx> <num_threads> <type>
Types: serial | static | nnzbal
```
## utils.h

The `utils.h` file contains the core **data structures** and **utility functions** used for managing sparse matrices in CSR format and performing both serial and parallel SpMV operations.

---

## Data Structures

### `CSR`

Represents a sparse matrix in **Compressed Sparse Row (CSR)** format.

| Field | Type | Description |
|-------|------|-------------|
| `size_t row` | Number of matrix rows |
| `size_t col` | Number of matrix columns |
| `size_t nnz` | Number of non-zero elements |
| `int *ro` | Row offsets | Index in `ci` / `val` where each row begins |
| `int *ci` | Column indices for each non-zero value |
| `float *val` | Non-zero values |

---

### `NNZBlock`

Used for load balancing during parallel execution by partitioning row in block based on the number of non-zero elements.

| Field | Description |
|-------|-------------|
| `int start_row` | First row assigned to the thread |
| `int end_row` | Last row assigned (exclusive) |

---

## Functions

### `CSR csr_from_mtx(const char *filename)`

Reads a **Matrix Market (.mtx)** file in COO format, sorts entries by row and column, and converts the matrix into **CSR format**.

**Processing steps:**
- Reads `(row, col, val)` triples  
- Converts from 1-based to 0-based indexing  
- Sorts COO entries using `qsort`  
- Builds CSR row offsets via prefix sum  
- Fills CSR arrays (`ro`, `ci`, `val`)  

**Parameters:**
- `filename` — Path to the `.mtx` file

**Returns:**  
A fully initialized `CSR` matrix.

---

### `NNZBlock* nnz_block_partition(CSR *m, int num_threads)`

Creates a load-balanced partition of matrix rows based on the number of non-zero elements per row.  
Each thread receives a block of rows with approximately the same total NNZ count.

**Parameters:**
- `m` — CSR matrix
- `num_threads` — Number of threads

**Returns:**  
A dynamically allocated array of `NNZBlock` of size `num_threads`.

---

### `float* generate_vector(size_t n)`

Generates a vector of `n` random floating-point numbers.

**Returns:**  
Pointer to a dynamically allocated array.

---

### `void csr_serial_spmv(CSR *m, const float *v, float *r)`

Performs **serial sparse matrix-vector multiplication (SpMV)** using CSR format.

**Parameters:**
- `m` — CSR matrix  
- `v` — Input vector  
- `r` — Output vector  

---

### `void csr_omp_static(CSR *m, const float *v, float *r, int num_threads)`

Performs SpMV in parallel using **OpenMP with static scheduling**.

**Parameters:**
- `m` — CSR matrix  
- `v` — Input vector  
- `r` — Output vector  
- `num_threads` — Number of OpenMP threads  

---

### `void csr_omp_nnz_balanced(CSR *m, const float *v, float *r, int num_threads, NNZBlock *parts)`

Parallel SpMV using **custom load balancing**, where the work is partitioned based on the number of non-zero elements per row.

Row blocks must be generated using `nnz_block_partition`.

**Parameters:**
- `m` — CSR matrix  
- `v` — Input vector  
- `r` — Output vector  
- `num_threads` — Number of threads  
- `parts` — Balanced row partitions  

---

### `bool compare_vectors(const float *a, const float *b, size_t n)`

Checks whether two vectors are approximately equal (tolerance `1e-6`).

**Returns:**
- `true` — if the vectors differ beyond the tolerance  
- `false` — if the vectors match  

---

### `double p90_time(double *times, int n)`

Computes the **90th percentile (P90)** from an array of execution times.

**Steps:**
- Sorts the array in ascending order  
- Returns the element at index `floor(0.9 · (n−1))`

---

