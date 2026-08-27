#ifndef __GRAPH_RR_H__
#define __GRAPH_RR_H__

#include "graph_csr.h"

typedef struct {
    uint32_t id;
    uint64_t degree;
} vertex_degree_t;

typedef struct {
    uint64_t degree; // Total degree of the community
    uint32_t child;  // Pointer/index to the head of the children list
} atom_t;

// Reordering vertices by degree
uint32_t* ordered_vertices_by_degree(csr_graph_t* graph);
uint64_t get_vertex_degree(csr_graph_t* graph, uint64_t u);

// Modularity gain
float compute_delta_q(uint64_t d_u, uint64_t d_v, uint64_t m, float w_uv);

// Compute Rabbit Order permutation: perm[old_id] = new_id, inv_perm[new_id] = old_id
int compute_rabbit_order(csr_graph_t *origin, uint32_t *perm, uint32_t *inv_perm);

// Apply permutation to create a reordered CSR graph
int csr_apply_permutation(csr_graph_t *origin, const uint32_t *perm, const uint32_t *inv_perm, csr_graph_t *result);

// Combined API to generate reordered graph using Rabbit Order
int csr_rabbit_order(csr_graph_t *origin, csr_graph_t *result, uint32_t *perm, uint32_t *inv_perm);

// Save CSR graph as edge list text file compatible with load_csr
int save_csr_as_edgelist(const char *filename, csr_graph_t *graph);

#endif // __GRAPH_RR_H__
