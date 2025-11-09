#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <stdbool.h>

typedef struct {
    size_t row, col, nnz;
    int *ro;     /* row offset */
    int *ci;     /* col index */
    float *val;  /* nnz values */
} CSR;

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
        fscanf(file, "%d %d %f", &row[k], &col[k], &val[k]);
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

    // Fix csr.ro (we incremented it while filling rows)
    for (int i = rows; i > 0; i--)
        csr.ro[i] = csr.ro[i - 1];
    csr.ro[0] = 0;

    free(coo);
    return csr;
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

void csr_omp_static(CSR *m, const float *v, float *r, int num_threads, int chunk_size) {
    omp_set_num_threads(num_threads);
    #pragma omp parallel for schedule(static, chunk_size)
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

void csr_omp_dynamic(CSR *m, const float *v, float *r, int num_threads, int chunk_size) {
    omp_set_num_threads(num_threads);
    #pragma omp parallel for schedule(dynamic, chunk_size)
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

void csr_omp_guided(CSR *m, const float *v, float *r, int num_threads, int chunk_size) {
    omp_set_num_threads(num_threads);
    #pragma omp parallel for schedule(guided, chunk_size)
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
