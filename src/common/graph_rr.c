#include "graph_rr.h"

int compare_vertex_degrees(const void *a, const void *b){
    const vertex_degree_t *v1 = (const vertex_degree_t *)a;
    const vertex_degree_t *v2 = (const vertex_degree_t *)b;

    if(v1->degree < v2->degree) return -1;
    if (v1->degree > v2->degree) return 1;
    return 0;
}

uint32_t* oredered_vertices_by_degree(csr_graph_t* graph){
    if (!graph || graph->num_vertices == 0) return NULL;

    vertex_degree_t *temp_info = malloc(graph->num_vertices * sizeof(vertex_degree_t));
    if(!temp_info) return NULL;

    for(int i = 0; i < graph->num_vertices; i++){
        temp_info[i].id = i;
        temp_info[i].degree = get_vertex_degree(graph, i);
    }

    //sort for degree
    qsort(temp_info, graph->num_vertices, sizeof(vertex_degree_t), compare_vertex_degrees);

    uint32_t *sorted_ids = malloc(graph->num_vertices*sizeof(uint32_t));
    if(sorted_ids){
        for(int i = 0; i< graph->num_vertices; i++){
            sorted_ids[i] = temp_info[i].id;
        }
    }

    free(temp_info);
    return sorted_ids;
}

uint64_t get_vertex_degree(csr_graph_t* graph, int u){
    return graph->row_ptr[u+1] - graph->row_ptr[u];

}

void generate_permutation_dfs(uint32_t current, atom_t* atoms, uint32_t* sibling, 
                              uint32_t* permutation, uint32_t* next_id) {
    // Traverse children first (depth-first)
    uint32_t child = atoms[current].child;
    while (child != UINT32_MAX) {
        generate_permutation_dfs(child, atoms, sibling, permutation, next_id);
        child = sibling[child];
    }
    // Assign new ID to the vertex itself
    permutation[current] = (*next_id)++;
}

float compute_delta_q(uint64_t d_u, uint64_t d_v, uint64_t m, float w_uv){
    float term1 = w_uv / (float)m;
    float term2 = (float)(d_u * d_v) / (float)(2.0 * m * m);
    return term1 - term2;
}

void csr_rabbit_prdering(csr_graph_t* origin, csr_graph_t* result){
    
}