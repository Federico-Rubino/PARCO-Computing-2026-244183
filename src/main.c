#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "utils.h"

int main(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr,"Usage: %s <file.mtx> <num_threads> <type> [chunk_size] [print]\n", argv[0]);
        return 1;
    }

    int pretty_print = 0;
    if (strcmp(argv[argc - 1], "print") == 0)
        pretty_print = 1;

    const char *filename = argv[1];
    int n_threads = atoi(argv[2]);
    const char *type = argv[3];
    int chunk_size = 0;

    if (strcmp(type, "serial") != 0) {
        if (argc < 5) {
            fprintf(stderr,"For parallel scheduling (static, dynamic, guided) give chunk size (>=1)\n");
            return 1;
        }
        chunk_size = atoi(argv[4]);
        if (chunk_size < 1) {
            fprintf(stderr,"Chunk size must be >= 1\n");
            return 1;
        }
    }

    srand(time(NULL));

    const char *file_only = strrchr(filename, '/');
    file_only = (file_only) ? file_only + 1 : filename;

    struct timespec t0, t1;
    double elapsed_time;

    CSR csr_matrix = csr_from_mtx(filename);

    float *x = generate_vector(csr_matrix.col);
    float *r_ref = malloc(csr_matrix.row * sizeof(float));
    if (!r_ref) {
        fprintf(stderr, "Memory allocation error\n");
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &t0);
    csr_serial_spmv(&csr_matrix, x, r_ref);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    elapsed_time = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;

    if(strcmp(type, "serial") == 0) {

        if (!pretty_print) {
            printf("%s,%zu,%zu,%zu,%d,%s,,%.9f\n",
                file_only, csr_matrix.row, csr_matrix.col, csr_matrix.nnz,
                n_threads, type, elapsed_time);
        } else {
            printf("File: %s\nRows: %zu\nCols: %zu\nNNZ: %zu\nThreads: %d\nType: %s\nTime: %.9f s\n",
                file_only, csr_matrix.row, csr_matrix.col, csr_matrix.nnz,
                n_threads, type, elapsed_time);
        }

    } else {

        float *r_parallel = malloc(csr_matrix.row * sizeof(float));
        if (!r_parallel) {
            fprintf(stderr, "Memory allocation error\n");
            return 1;
        }

        if(strcmp(type, "static") == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t0);
            csr_omp_static(&csr_matrix, x, r_parallel, n_threads, chunk_size);
            clock_gettime(CLOCK_MONOTONIC, &t1);
        } else if(strcmp(type, "dynamic") == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t0);
            csr_omp_dynamic(&csr_matrix, x, r_parallel, n_threads, chunk_size);
            clock_gettime(CLOCK_MONOTONIC, &t1);
        } else if(strcmp(type, "guided") == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t0);
            csr_omp_guided(&csr_matrix, x, r_parallel, n_threads, chunk_size);
            clock_gettime(CLOCK_MONOTONIC, &t1);
        } else {
            fprintf(stderr, "Unknown type: %s\n", type);
            free(r_parallel);
            return 1;
        }

        elapsed_time = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;

        if(compare_vectors(r_ref, r_parallel, csr_matrix.row) == 1) {
            fprintf(stderr, "Serial and parallel results do not match!\n");
            free(r_parallel);
            return 1;
        }

        if (!pretty_print) {
            printf("%s,%zu,%zu,%zu,%d,%s,%d,%.9f\n",
                file_only, csr_matrix.row, csr_matrix.col, csr_matrix.nnz,
                n_threads, type, chunk_size, elapsed_time);
        } else {
            printf("File: %s\nRows: %zu\nCols: %zu\nNNZ: %zu\nThreads: %d\nType: %s\nChunk: %d\nTime: %.9f s\n",
                file_only, csr_matrix.row, csr_matrix.col, csr_matrix.nnz,
                n_threads, type, chunk_size, elapsed_time);
        }

        free(r_parallel);
    }

    free(csr_matrix.ro);
    free(csr_matrix.ci);
    free(csr_matrix.val);
    free(x);
    free(r_ref);

    return 0;
}
