#include "graph_rr.h"
#include <string.h>
#include <inttypes.h>

int compare_vertex_degrees_desc(const void *a, const void *b){
    const vertex_degree_t *v1 = (const vertex_degree_t *)a;
    const vertex_degree_t *v2 = (const vertex_degree_t *)b;

    if(v1->degree > v2->degree) return -1;
    if(v1->degree < v2->degree) return 1;
    return 0;
}

uint64_t get_vertex_degree(csr_graph_t* graph, uint64_t u){
    if(!graph || u >= graph->num_vertices) return 0;
    return graph->row_ptr[u + 1] - graph->row_ptr[u];
}

uint32_t* ordered_vertices_by_degree(csr_graph_t* graph){
    if(!graph || graph->num_vertices == 0) return NULL;

    vertex_degree_t *temp_info = malloc(graph->num_vertices * sizeof(vertex_degree_t));
    if(!temp_info) return NULL;

    for(uint64_t i = 0; i < graph->num_vertices; i++){
        temp_info[i].id = (uint32_t)i;
        temp_info[i].degree = get_vertex_degree(graph, i);
    }

    qsort(temp_info, graph->num_vertices, sizeof(vertex_degree_t), compare_vertex_degrees_desc);

    uint32_t *sorted_ids = malloc(graph->num_vertices * sizeof(uint32_t));
    if(sorted_ids){
        for(uint64_t i = 0; i < graph->num_vertices; i++){
            sorted_ids[i] = temp_info[i].id;
        }
    }

    free(temp_info);
    return sorted_ids;
}

float compute_delta_q(uint64_t d_u, uint64_t d_v, uint64_t m, float w_uv){
    if(m == 0) return 0.0f;
    double m_d = (double)m;
    double term1 = (double)w_uv / m_d;
    double term2 = ((double)d_u * (double)d_v) / (2.0 * m_d * m_d);
    return (float)(term1 - term2);
}

int compute_rabbit_order(csr_graph_t *origin, uint32_t *perm, uint32_t *inv_perm){
    if(!origin || !perm || !inv_perm) return -1;

    uint64_t n = origin->num_vertices;
    uint64_t m = origin->num_edges;

    if(n == 0){
        return 0;
    }

    atom_t *atoms = malloc(n * sizeof(atom_t));
    uint32_t *sibling = malloc(n * sizeof(uint32_t));
    uint32_t *community = malloc(n * sizeof(uint32_t));
    uint8_t *is_merged = calloc(n, sizeof(uint8_t));

    if(!atoms || !sibling || !community || !is_merged){
        free(atoms); free(sibling); free(community); free(is_merged);
        return -1;
    }

    for(uint64_t i = 0; i < n; i++){
        atoms[i].degree = origin->row_ptr[i + 1] - origin->row_ptr[i];
        atoms[i].child = UINT32_MAX;
        sibling[i] = UINT32_MAX;
        community[i] = (uint32_t)i;
    }

    uint32_t *ordered_nodes = ordered_vertices_by_degree(origin);
    if(!ordered_nodes){
        free(atoms); free(sibling); free(community); free(is_merged);
        return -1;
    }

    // Community aggregation based on modularity gain delta Q
    for(uint64_t idx = 0; idx < n; idx++){
        uint32_t u = ordered_nodes[idx];
        uint64_t deg_u = atoms[u].degree; // current accumulated degree, not the raw original one
        if(deg_u == 0) continue;

        uint32_t best_c = UINT32_MAX;
        float best_gain = 0.0f;

        for(uint64_t e = origin->row_ptr[u]; e < origin->row_ptr[u + 1]; e++){
            uint32_t v = origin->col_idx[e];
            uint32_t cv = community[v];
            if(cv == community[u]) continue;
            // merge only into a strictly larger community, or no root survives
            if(atoms[cv].degree <= deg_u) continue;

            float gain = compute_delta_q(deg_u, atoms[cv].degree, m, origin->weights[e]);
            if(gain > best_gain){
                best_gain = gain;
                best_c = cv;
            }
        }

        if(best_gain > 0.0f && best_c != UINT32_MAX && best_c != u){
            // Merge u into best_c community tree
            atoms[best_c].degree += deg_u;
            sibling[u] = atoms[best_c].child;
            atoms[best_c].child = u;
            community[u] = best_c;
            is_merged[u] = 1;
        }
    }

    free(ordered_nodes);

    // Iterative DFS dendrogram traversal to avoid recursion stack overflow
    uint32_t *stack = malloc(n * sizeof(uint32_t));
    uint8_t *visited = calloc(n, sizeof(uint8_t));
    uint32_t next_id = 0;

    if(!stack || !visited){
        free(atoms); free(sibling); free(community); free(is_merged);
        free(stack); free(visited);
        return -1;
    }

    for(uint64_t i = 0; i < n; i++){
        if(is_merged[i] == 0){
            int top = 0;
            stack[top] = (uint32_t)i;

            while(top >= 0){
                uint32_t curr = stack[top--];
                if(visited[curr]) continue;

                perm[curr] = next_id;
                inv_perm[next_id] = curr;
                next_id++;
                visited[curr] = 1;

                // Push siblings and children
                uint32_t child = atoms[curr].child;
                while(child != UINT32_MAX){
                    if(!visited[child]){
                        stack[++top] = child;
                    }
                    child = sibling[child];
                }
            }
        }
    }

    // Safety fallback for any remaining nodes
    for(uint64_t i = 0; i < n; i++){
        if(!visited[i]){
            perm[i] = next_id;
            inv_perm[next_id] = (uint32_t)i;
            next_id++;
            visited[i] = 1;
        }
    }

    free(atoms);
    free(sibling);
    free(community);
    free(is_merged);
    free(stack);
    free(visited);

    return 0;
}

int csr_apply_permutation(csr_graph_t *origin, const uint32_t *perm, const uint32_t *inv_perm, csr_graph_t *result){
    if(!origin || !perm || !inv_perm || !result) return -1;

    uint64_t n = origin->num_vertices;
    uint64_t m = origin->num_edges;

    result->num_vertices = n;
    result->num_edges = m;

    result->row_ptr = malloc((n + 1) * sizeof(uint64_t));
    result->col_idx = malloc(m * sizeof(uint32_t));
    result->weights = malloc(m * sizeof(float));

    if(!result->row_ptr || (!result->col_idx && m > 0) || (!result->weights && m > 0)){
        free(result->row_ptr); free(result->col_idx); free(result->weights);
        return -1;
    }

    result->row_ptr[0] = 0;
    for(uint64_t new_u = 0; new_u < n; new_u++){
        uint32_t old_u = inv_perm[new_u];
        uint64_t deg = origin->row_ptr[old_u + 1] - origin->row_ptr[old_u];
        result->row_ptr[new_u + 1] = result->row_ptr[new_u] + deg;
    }

    for(uint64_t new_u = 0; new_u < n; new_u++){
        uint32_t old_u = inv_perm[new_u];
        uint64_t old_start = origin->row_ptr[old_u];
        uint64_t old_end = origin->row_ptr[old_u + 1];
        uint64_t new_start = result->row_ptr[new_u];

        for(uint64_t e = old_start; e < old_end; e++){
            uint32_t old_v = origin->col_idx[e];
            uint32_t new_v = perm[old_v];
            uint64_t offset = e - old_start;
            result->col_idx[new_start + offset] = new_v;
            result->weights[new_start + offset] = origin->weights[e];
        }
    }

    return 0;
}

int csr_rabbit_order(csr_graph_t *origin, csr_graph_t *result, uint32_t *perm, uint32_t *inv_perm){
    if(!origin || !result) return -1;

    int alloc_perm = 0;
    if(!perm){
        perm = malloc(origin->num_vertices * sizeof(uint32_t));
        alloc_perm = 1;
    }

    int alloc_inv = 0;
    if(!inv_perm){
        inv_perm = malloc(origin->num_vertices * sizeof(uint32_t));
        alloc_inv = 1;
    }

    if(!perm || !inv_perm){
        if(alloc_perm) free(perm);
        if(alloc_inv) free(inv_perm);
        return -1;
    }

    if(compute_rabbit_order(origin, perm, inv_perm) != 0){
        if(alloc_perm) free(perm);
        if(alloc_inv) free(inv_perm);
        return -1;
    }

    int ret = csr_apply_permutation(origin, perm, inv_perm, result);

    if(alloc_perm) free(perm);
    if(alloc_inv) free(inv_perm);

    return ret;
}

int save_csr_as_edgelist(const char *filename, csr_graph_t *graph){
    if(!filename || !graph) return -1;

    FILE *f = fopen(filename, "w");
    if(!f) return -1;

    fprintf(f, "# Reordered graph: %" PRIu64 " vertices, %" PRIu64 " edges\n", graph->num_vertices, graph->num_edges);
    for(uint64_t u = 0; u < graph->num_vertices; u++){
        uint64_t start = graph->row_ptr[u];
        uint64_t end = graph->row_ptr[u + 1];
        for(uint64_t e = start; e < end; e++){
            uint32_t v = graph->col_idx[e];
            float w = graph->weights[e];
            fprintf(f, "%" PRIu64 " %u %.4f\n", u, v, w);
        }
    }

    fclose(f);
    return 0;
}