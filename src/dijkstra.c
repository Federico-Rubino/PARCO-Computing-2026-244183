#define _POSIX_C_SOURCE 199309L
#include "common/graph_csr.h"
#include <math.h>
#include <time.h>
#include <inttypes.h>

typedef struct {
    float dist;
    uint32_t vertex;
} heap_node_t;

static inline void heap_swap(heap_node_t *a, heap_node_t *b){
    heap_node_t t = *a; *a = *b; *b = t;
}

static void heap_push(heap_node_t *heap, size_t *size, float dist, uint32_t vertex){
    size_t i = (*size)++;
    heap[i].dist = dist;
    heap[i].vertex = vertex;
    while(i > 0){
        size_t parent = (i - 1) / 2;
        if(heap[parent].dist <= heap[i].dist) break;
        heap_swap(&heap[parent], &heap[i]);
        i = parent;
    }
}

static heap_node_t heap_pop(heap_node_t *heap, size_t *size){
    heap_node_t top = heap[0];
    heap[0] = heap[--(*size)];
    size_t i = 0;
    for(;;){
        size_t l = 2 * i + 1, r = 2 * i + 2, smallest = i;
        if(l < *size && heap[l].dist < heap[smallest].dist) smallest = l;
        if(r < *size && heap[r].dist < heap[smallest].dist) smallest = r;
        if(smallest == i) break;
        heap_swap(&heap[i], &heap[smallest]);
        i = smallest;
    }
    return top;
}

static void dijkstra_init(csr_graph_t *g, uint32_t source, float *dist, heap_node_t *heap, size_t *heap_size){
    for(uint64_t i = 0; i < g->num_vertices; i++) dist[i] = INFINITY;
    dist[source] = 0.0f;
    *heap_size = 0;
    heap_push(heap, heap_size, 0.0f, source);
}

static void dijkstra(csr_graph_t *g, float *dist, heap_node_t *heap, size_t heap_size){
    while(heap_size > 0){
        heap_node_t top = heap_pop(heap, &heap_size);
        if(top.dist > dist[top.vertex]) continue;
        uint32_t u = top.vertex;
        for(uint64_t e = g->row_ptr[u]; e < g->row_ptr[u + 1]; e++){
            uint32_t v = g->col_idx[e];
            float nd = dist[u] + g->weights[e];
            if(nd < dist[v]){
                dist[v] = nd;
                heap_push(heap, &heap_size, nd, v);
            }
        }
    }
}

static double now_sec(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv){
    if(argc < 2){
        fprintf(stderr, "usage: %s <graph_file> [undirected=0] [source=0] [num_runs=1] [dist_out_file]\n", argv[0]);
        return 1;
    }
    const char *filename = argv[1];
    int undirected = argc > 2 ? atoi(argv[2]) : 0;
    uint32_t source = argc > 3 ? (uint32_t)strtoul(argv[3], NULL, 10) : 0;
    int num_runs = argc > 4 ? atoi(argv[4]) : 1;
    const char *dist_out = argc > 5 ? argv[5] : NULL;

    csr_graph_t g;
    if(load_csr(filename, &g, undirected) != 0){
        fprintf(stderr, "failed to load %s\n", filename);
        return 1;
    }

    float *dist = malloc(g.num_vertices * sizeof(float));
    // upper bound: 1 initial push + at most one push per edge relaxation
    heap_node_t *heap = malloc((g.num_edges + 1) * sizeof(heap_node_t));
    if(!dist || !heap){
        fprintf(stderr, "allocation failed\n");
        return 1;
    }

    printf("run,time_ms\n");
    for(int run = 1; run <= num_runs; run++){
        size_t heap_size;
        dijkstra_init(&g, source, dist, heap, &heap_size);

        double start = now_sec();
        dijkstra(&g, dist, heap, heap_size);
        double end = now_sec();

        printf("%d,%.6f\n", run, (end - start) * 1000.0);
    }

    if(dist_out){
        FILE *f = fopen(dist_out, "w");
        if(f){
            for(uint64_t v = 0; v < g.num_vertices; v++)
                fprintf(f, "%" PRIu64 " %f\n", v, dist[v]);
            fclose(f);
        }
    }

    free(dist);
    free(heap);
    free_csr(&g);
    return 0;
}
