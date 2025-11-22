#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "utils.h"

#define SIMNUM 10

int main(int argc, char* argv[]) {

    if (argc < 4) {
        fprintf(stderr,"Usage: %s <file.mtx> <num_threads> <type>\n", argv[0]);
        fprintf(stderr,"Types: serial | static | nnzbal\n");
        return 1;
    }

    const char *filename = argv[1];
    int n_threads = atoi(argv[2]);
    const char *type = argv[3];


    srand(time(NULL));

    struct timespec t0, t1;

    CSR csr_matrix = csr_from_mtx(filename);
    if (csr_matrix.row == 0 || csr_matrix.col == 0) {
        fprintf(stderr, "Error reading matrix file %s\n", filename);
        return 1;
    }

    double p90;
    float *x        = generate_vector(csr_matrix.col);
    float *r_ref    = malloc(csr_matrix.row * sizeof(float));
    float *r_result = malloc(csr_matrix.row * sizeof(float));
    double *times   = malloc(SIMNUM * sizeof(double));

    if (!x || !r_ref || !r_result || !times) {
        fprintf(stderr,"Memory allocation error\n");
        return 1;
    }

    // SERIAL REFERENCE 
    csr_serial_spmv(&csr_matrix, x, r_ref);


    // SERIAL 
    if (strcmp(type, "serial") == 0) {

        csr_serial_spmv(&csr_matrix, x, r_result);

        for (int i = 0; i < SIMNUM; i++) {
            clock_gettime(CLOCK_MONOTONIC, &t0);
            csr_serial_spmv(&csr_matrix, x, r_result);
            clock_gettime(CLOCK_MONOTONIC, &t1);

            times[i] = (t1.tv_sec - t0.tv_sec)
                     + (t1.tv_nsec - t0.tv_nsec) / 1e9;

            if (compare_vectors(r_ref, r_result, csr_matrix.row)) {
                fprintf(stderr,"Results do not match reference!\n");
                goto cleanup_error;
            }
        }

        p90 = p90_time(times, SIMNUM);
        printf("%d,%s,%.9f\n", n_threads, type, p90);
    }

    // STATIC 
    else if (strcmp(type, "static") == 0) {

        csr_omp_static(&csr_matrix, x, r_result, n_threads);

        for (int i = 0; i < SIMNUM; i++) {
            clock_gettime(CLOCK_MONOTONIC, &t0);
            csr_omp_static(&csr_matrix, x, r_result, n_threads);
            clock_gettime(CLOCK_MONOTONIC, &t1);

            times[i] = (t1.tv_sec - t0.tv_sec)
                     + (t1.tv_nsec - t0.tv_nsec) / 1e9;

            if (compare_vectors(r_ref, r_result, csr_matrix.row)) {
                fprintf(stderr,"Results do not match reference!\n");
                goto cleanup_error;
            }
        }

        p90 = p90_time(times, SIMNUM);
        printf("%d,%s,%.9f\n", n_threads, type, p90);
    }


    // NNZ BALANCED 
    else if (strcmp(type, "nnzbal") == 0) {

        NNZBlock *parts = nnz_block_partition(&csr_matrix, n_threads);

        csr_omp_nnz_balanced(&csr_matrix, x, r_result, n_threads, parts);

        for (int i = 0; i < SIMNUM; i++) {
            clock_gettime(CLOCK_MONOTONIC, &t0);
            csr_omp_nnz_balanced(&csr_matrix, x, r_result, n_threads, parts);
            clock_gettime(CLOCK_MONOTONIC, &t1);

            times[i] = (t1.tv_sec - t0.tv_sec)
                     + (t1.tv_nsec - t0.tv_nsec) / 1e9;

            if (compare_vectors(r_ref, r_result, csr_matrix.row)) {
                fprintf(stderr,"Results do not match reference!\n");
                free(parts);
                goto cleanup_error;
            }
        }

        p90 = p90_time(times, SIMNUM);
        printf("%d,%s,%.9f\n", n_threads, type, p90);

        free(parts);
    }

    else {
        fprintf(stderr,"Unknown type: %s\n", type);
        goto cleanup_error;
    }


    free(csr_matrix.ro);
    free(csr_matrix.ci);
    free(csr_matrix.val);
    free(x);
    free(r_ref);
    free(r_result);
    free(times);

    return 0;

cleanup_error:
    free(csr_matrix.ro);
    free(csr_matrix.ci);
    free(csr_matrix.val);
    free(x);
    free(r_ref);
    free(r_result);
    free(times);
    return 1;
}
