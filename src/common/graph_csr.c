#include "graph_csr.h"
#include <string.h>
#include <inttypes.h>

#define SNAP_WEIGHT_MIN 1
#define SNAP_WEIGHT_MAX 20
#define SNAP_WEIGHT_SEED 42

int load_csr(const char *filename, csr_graph_t *graph, int undirected){
    FILE *f = fopen(filename, "r");
    if(!f) return -1;

    srand(SNAP_WEIGHT_SEED);

    char line[512];
    uint64_t max_id = 0;
    uint64_t edge_count = 0;

    while(fgets(line, sizeof(line), f)){
        if(line[0] == '#' || line[0] == '\n') continue;
        uint64_t u, v;
        if(sscanf(line, "%" SCNu64 " %" SCNu64, &u, &v) != 2) continue;
        if(u > max_id) max_id = u;
        if(v > max_id) max_id = v;
        edge_count++;
    }

    if(edge_count == 0){ fclose(f); return -1; }

    uint64_t *edge_u = malloc(edge_count * sizeof(uint64_t));
    uint64_t *edge_v = malloc(edge_count * sizeof(uint64_t));
    float *edge_w = malloc(edge_count * sizeof(float));
    if(!edge_u || !edge_v || !edge_w){
        free(edge_u); free(edge_v); free(edge_w);
        fclose(f);
        return -1;
    }

    rewind(f);
    uint64_t idx = 0;
    while(fgets(line, sizeof(line), f)){
        if(line[0] == '#' || line[0] == '\n') continue;
        uint64_t u, v;
        float w;
        int n = sscanf(line, "%" SCNu64 " %" SCNu64 " %f", &u, &v, &w);
        if(n < 2) continue;
        edge_u[idx] = u;
        edge_v[idx] = v;
        edge_w[idx] = (n == 3) ? w : (float)(SNAP_WEIGHT_MIN + rand() % (SNAP_WEIGHT_MAX - SNAP_WEIGHT_MIN + 1));
        idx++;
    }
    fclose(f);
    edge_count = idx;

    // ids from SNAP files are not guaranteed contiguous
    uint64_t num_vertices = max_id + 1;
    uint64_t total_edges = undirected ? edge_count * 2 : edge_count;

    uint64_t *degree = calloc(num_vertices, sizeof(uint64_t));
    for(uint64_t i = 0; i < edge_count; i++){
        degree[edge_u[i]]++;
        if(undirected) degree[edge_v[i]]++;
    }

    uint64_t *row_ptr = malloc((num_vertices + 1) * sizeof(uint64_t));
    row_ptr[0] = 0;
    for(uint64_t i = 0; i < num_vertices; i++){
        row_ptr[i + 1] = row_ptr[i] + degree[i];
    }

    uint32_t *col_idx = malloc(total_edges * sizeof(uint32_t));
    float *weights = malloc(total_edges * sizeof(float));
    uint64_t *cursor = malloc(num_vertices * sizeof(uint64_t));
    memcpy(cursor, row_ptr, num_vertices * sizeof(uint64_t));

    for(uint64_t i = 0; i < edge_count; i++){
        uint64_t u = edge_u[i], v = edge_v[i];
        float w = edge_w[i];

        col_idx[cursor[u]] = (uint32_t)v;
        weights[cursor[u]] = w;
        cursor[u]++;

        if(undirected){
            col_idx[cursor[v]] = (uint32_t)u;
            weights[cursor[v]] = w;
            cursor[v]++;
        }
    }

    free(edge_u); free(edge_v); free(edge_w);
    free(degree); free(cursor);

    graph->num_vertices = num_vertices;
    graph->num_edges = total_edges;
    graph->row_ptr = row_ptr;
    graph->col_idx = col_idx;
    graph->weights = weights;

    return 0;
}

void free_csr(csr_graph_t *graph){
    free(graph->row_ptr);
    free(graph->col_idx);
    free(graph->weights);
    graph->row_ptr = NULL;
    graph->col_idx = NULL;
    graph->weights = NULL;
}
