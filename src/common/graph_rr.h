#ifndef __GRAPH_RR_H__
#define __GRAPH_RR_H__

#include "graph_csr.h"

typedef struct {
    uint32_t id;
    uint64_t degree;
} vertex_degree_t;

typedef struct {
    uint64_t degree; //Total degree of the community
    uint32_t child; // POinter to the head of the children list
} atom_t;

//reordering vertices by degree
uint32_t* oredered_vertices_by_degree(csr_graph_t* graph);
uint64_t get_vertex_degree(csr_graph_t* graph, int u);

//API functions
void csr_rabbit_prdering(csr_graph_t* origin, csr_graph_t* result);



#endif //__GRAPH_RR_H__
