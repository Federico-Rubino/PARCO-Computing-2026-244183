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

### Data Structures

#### `CSR`

Represents a sparse matrix in **Compressed Sparse Row (CSR)** format.

| Field | Type | Description |
|-------|------|-------------|
| `size_t row` | Number of rows in the matrix |  |
| `size_t col` | Number of columns in the matrix |  |
| `size_t nnz` | Number of non-zero elements |  |
| `int *ro` | Row offset array | Stores the starting index of each row in `ci` and `val` arrays |
| `int *ci` | Column indices | Column indices corresponding to non-zero values |
| `float *val` | Non-zero values | Actual values of the matrix |

---

### Functions

#### `CSR csr_from_mtx(const char *filename)`

Reads a matrix from a **Matrix Market (.mtx)** file and converts it to **CSR format**.

**Parameters:**

- `filename` – Path to the Matrix Market file.

**Returns:**

- A `CSR` structure representing the matrix.

---

#### `float* generate_vector(size_t n)`

Generates a random vector of floats of size `n`.

**Parameters:**

- `n` – Number of elements in the vector.

**Returns:**

- Pointer to a dynamically allocated array of floats.

---

#### `void csr_serial_spmv(CSR *m, const float *v, float *r)`

Performs **sparse matrix-vector multiplication (SpMV)** using the **CSR format** in serial.

**Parameters:**

- `m` – Pointer to the CSR matrix.
- `v` – Input vector.
- `r` – Output vector (result of `m * v`).

---

#### `void csr_omp_static(CSR *m, const float *v, float *r, int num_threads, int chunk_size)`

Performs CSR SpMV in **parallel using OpenMP with static scheduling**.

**Parameters:**

- `m` – Pointer to the CSR matrix.
- `v` – Input vector.
- `r` – Output vector.
- `num_threads` – Number of OpenMP threads.
- `chunk_size` – Chunk size for static scheduling.

---

#### `void csr_omp_dynamic(CSR *m, const float *v, float *r, int num_threads, int chunk_size)`

Performs CSR SpMV in **parallel using OpenMP with dynamic scheduling**.

**Parameters:**

- `m` – Pointer to the CSR matrix.
- `v` – Input vector.
- `r` – Output vector.
- `num_threads` – Number of OpenMP threads.
- `chunk_size` – Chunk size for dynamic scheduling.

---

#### `void csr_omp_guided(CSR *m, const float *v, float *r, int num_threads, int chunk_size)`

Performs CSR SpMV in **parallel using OpenMP with guided scheduling**.

**Parameters:**

- `m` – Pointer to the CSR matrix.
- `v` – Input vector.
- `r` – Output vector.
- `num_threads` – Number of OpenMP threads.
- `chunk_size` – Chunk size for guided scheduling.

---

#### `bool compare_vectors(const float *a, const float *b, size_t n)`

Compares two vectors element-wise to check if they are approximately equal.

**Parameters:**

- `a` – First vector.
- `b` – Second vector.
- `n` – Number of elements.

**Returns:**

- `true` if vectors differ by more than `1e-6` at any position, otherwise `false`.

---