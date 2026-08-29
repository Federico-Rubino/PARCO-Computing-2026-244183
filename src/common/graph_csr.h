#ifndef __GRAPH_CSR_H__
#define __GRAPH_CSR_H__

#include "utils.h"

typedef struct {
    uint64_t num_vertices;
    uint64_t num_edges;

    uint64_t *row_ptr;   // size: num_vertices + 1
    uint32_t *col_idx;   // size: num_edges
    float    *weights;   // size: num_edges
} csr_graph_t;

int load_csr(const char *filename, csr_graph_t *graph, int undirected);
void free_csr(csr_graph_t *graph);
uint32_t max_out_degree_vertex(csr_graph_t *graph);

#endif //__GRAPH_CSR_H__