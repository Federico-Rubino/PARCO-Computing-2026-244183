#define _POSIX_C_SOURCE 199309L
#include "common/graph_csr.h"
#include "common/graph_rr.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>

int main(int argc, char **argv){
    if(argc < 3){
        fprintf(stderr, "usage: %s <input_graph> <output_graph> [undirected=0] [perm_out_file]\n", argv[0]);
        return 1;
    }

    const char *input_file = argv[1];
    const char *output_file = argv[2];
    int undirected = argc > 3 ? atoi(argv[3]) : 0;
    const char *perm_out = argc > 4 ? argv[4] : NULL;

    printf("Loading graph from %s (undirected=%d)...\n", input_file, undirected);
    csr_graph_t origin;
    if(load_csr(input_file, &origin, undirected) != 0){
        fprintf(stderr, "Failed to load graph from %s\n", input_file);
        return 1;
    }
    printf("Loaded: %" PRIu64 " vertices, %" PRIu64 " edges\n", origin.num_vertices, origin.num_edges);

    uint32_t *perm = malloc(origin.num_vertices * sizeof(uint32_t));
    uint32_t *inv_perm = malloc(origin.num_vertices * sizeof(uint32_t));

    struct timespec ts1, ts2;
    clock_gettime(CLOCK_MONOTONIC, &ts1);

    printf("Computing Rabbit Order...\n");
    if(compute_rabbit_order(&origin, perm, inv_perm) != 0){
        fprintf(stderr, "Failed to compute Rabbit Order\n");
        return 1;
    }

    printf("Applying permutation...\n");
    csr_graph_t reordered;
    if(csr_apply_permutation(&origin, perm, inv_perm, &reordered) != 0){
        fprintf(stderr, "Failed to apply permutation\n");
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts2);
    double elapsed_ms = (ts2.tv_sec - ts1.tv_sec) * 1000.0 + (ts2.tv_nsec - ts1.tv_nsec) * 1e-6;
    printf("Rabbit Order computed and applied in %.2f ms\n", elapsed_ms);

    printf("Saving reordered graph to %s...\n", output_file);
    if(save_csr_as_edgelist(output_file, &reordered) != 0){
        fprintf(stderr, "Failed to save reordered graph\n");
        return 1;
    }

    if(perm_out){
        printf("Saving permutation mapping to %s...\n", perm_out);
        FILE *pf = fopen(perm_out, "w");
        if(pf){
            for(uint64_t i = 0; i < origin.num_vertices; i++){
                fprintf(pf, "%" PRIu64 " %u\n", i, perm[i]);
            }
            fclose(pf);
        }
    }

    free_csr(&reordered);
    free_csr(&origin);
    free(perm);
    free(inv_perm);

    printf("Done!\n");
    return 0;
}
