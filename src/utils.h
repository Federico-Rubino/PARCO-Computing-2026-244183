#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <stdbool.h>

typedef struct {
    size_t row, col, nnz;
    int *ro;     // row offsets
    int *ci;     // col indices
    float *val;  // nnz values
} CSR;

typedef struct {
    int start_row;
    int end_row; //exclusive value
} NNZBlock;

typedef struct {
        int row, col;
        float val;
} coo_t;


    // Comparison function for qsort
int cmp_coo(const void *a, const void *b) {
    const coo_t *x = a, *y = b;
    if (x->row != y->row) return x->row - y->row;
    return x->col - y->col;
}

CSR csr_from_mtx(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "File open error\n");
        exit(EXIT_FAILURE);
    }

    char line[256];
    do {
        if (!fgets(line, sizeof(line), file)) {
            fprintf(stderr, "Invalid file format\n");
            exit(EXIT_FAILURE);
        }
    } while (line[0] == '%');

    // Read matrix size
    int rows, cols, nnz;
    sscanf(line, "%d %d %d", &rows, &cols, &nnz);

    // Temporary COO arrays
    int *row = malloc(nnz * sizeof(int));
    int *col = malloc(nnz * sizeof(int));
    float *val = malloc(nnz * sizeof(float));

    // Read COO
    for (int k = 0; k < nnz; k++) {
        if (fscanf(file, "%d %d %f", &row[k], &col[k], &val[k]) != 3) {
            fprintf(stderr, "Error reading COO entry %d in file %s\n", k, filename);
            exit(EXIT_FAILURE);
        }
    row[k]--;   // 0-based index
    col[k]--;
    }
    fclose(file);

    coo_t *coo = malloc(nnz * sizeof(coo_t));
    for (int k = 0; k < nnz; k++) {
        coo[k].row = row[k];
        coo[k].col = col[k];
        coo[k].val = val[k];
    }

    free(row);
    free(col);
    free(val);


    qsort(coo, nnz, sizeof(coo_t), cmp_coo); //sort COO

    CSR csr;
    csr.row = rows;
    csr.col = cols;
    csr.nnz = nnz;
    csr.ro  = calloc(rows + 1, sizeof(int));
    csr.ci  = malloc(nnz * sizeof(int));
    csr.val = malloc(nnz * sizeof(float));

    // Count entries per row
    for (int k = 0; k < nnz; k++)
        csr.ro[coo[k].row + 1]++;

    // Prefix sum → CSR row offsets
    for (int i = 0; i < rows; i++)
        csr.ro[i + 1] += csr.ro[i];

    // Fill CSR
    for (int k = 0; k < nnz; k++) {
        int r = coo[k].row;
        int idx = csr.ro[r]++;
        csr.ci[idx] = coo[k].col;
        csr.val[idx] = coo[k].val;
    }

    // Fix csr.ro
    for (int i = rows; i > 0; i--)
        csr.ro[i] = csr.ro[i - 1];
    csr.ro[0] = 0;

    free(coo);
    return csr;
}

NNZBlock *nnz_block_partition(CSR *m, int num_threads){
    NNZBlock *parts = malloc(num_threads * sizeof(NNZBlock));
    size_t target = m->nnz / num_threads;

    int p = 0;
    size_t acc = 0;
    parts[p].start_row = 0;

    for (int i = 0; i < m->row; i++) {
        size_t rnnz = m->ro[i+1] - m->ro[i];
        acc += rnnz;

        if (acc >= target && p < num_threads - 1) {
            parts[p].end_row = i + 1;
            p++;
            parts[p].start_row = i + 1;
            acc = 0;
        }
    }

    parts[p].end_row = m->row;
    return parts;
}

float* generate_vector(size_t n) {
    float *v = (float*)malloc(n * sizeof(float));
    for (size_t i = 0; i < n; i++)
        v[i] = (float)rand() / RAND_MAX;
    return v;
}

void csr_serial_spmv(CSR *m, const float *v, float *r) {
    for (size_t i = 0; i < m->row; i++) {
        float sum = 0.0f;
        int start_index = m->ro[i];
        int end_index = m->ro[i + 1];
        for (int j = start_index; j < end_index; j++){
            sum += m->val[j] * v[m->ci[j]];
        }
        r[i] = sum;
    }
}

void csr_omp_static(CSR *m, const float *v, float *r, int num_threads) {
    omp_set_num_threads(num_threads);
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < m->row; i++) {
        float sum = 0.0f;
        int start_index = m->ro[i];
        int end_index = m->ro[i + 1];
        for (int j = start_index; j < end_index; j++){
            sum += m->val[j] * v[m->ci[j]];
        }
        r[i] = sum;
    }
}


void csr_omp_nnz_balanced(CSR *m, const float *x, float *r,
                          int num_threads, NNZBlock *parts)
{
    #pragma omp parallel num_threads(num_threads)
    {
        int tid = omp_get_thread_num();
        int start = parts[tid].start_row;
        int end   = parts[tid].end_row;

        for (int i = start; i < end; i++) {
            float sum = 0.0f;
            int begin = m->ro[i];
            int stop  = m->ro[i+1];

            for (int j = begin; j < stop; j++){
                sum += m->val[j] * x[m->ci[j]];
            }

            r[i] = sum;
        }
    }
}


bool compare_vectors(const float *a, const float *b, size_t n) {
    const float eps = 1e-6f;
    for (size_t i = 0; i < n; i++) {
        float diff = a[i] - b[i];
        if (diff < 0) diff = -diff;
        if (diff > eps) {
            return 1; 
        }
    }
    return 0; 
}

static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db); // returns -1, 0, 1
}

double p90_time(double *times, int n) {
    if (n <= 0) return 0.0;

    // Sort the array in ascending order
    qsort(times, n, sizeof(double), cmp_double);

    // get 90p index: 0.9*(n-1)
    int index = (int)(0.9 * (n - 1));

    return times[index];
}