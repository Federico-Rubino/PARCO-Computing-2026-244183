#define _POSIX_C_SOURCE 199309L
#include "common/graph_csr.h"
#include <math.h>
#include <time.h>
#include <inttypes.h>
#include <string.h>

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
        fprintf(stderr, "usage: %s <graph_file> [undirected=0] [source=auto] [warmup_runs=5] [num_runs=10] [csv_out_file] [dist_out_file]\n", argv[0]);
        return 1;
    }
    const char *filename = argv[1];
    int undirected = argc > 2 ? atoi(argv[2]) : 0;
    const char *source_arg = argc > 3 ? argv[3] : "auto";
    int warmup_runs = argc > 4 ? atoi(argv[4]) : 5;
    int num_runs = argc > 5 ? atoi(argv[5]) : 10;
    const char *csv_out = argc > 6 ? argv[6] : NULL;
    const char *dist_out = argc > 7 ? argv[7] : NULL;

    csr_graph_t g;
    if(load_csr(filename, &g, undirected) != 0){
        fprintf(stderr, "failed to load %s\n", filename);
        return 1;
    }

    uint32_t source;
    if(strcmp(source_arg, "auto") == 0){
        source = max_out_degree_vertex(&g);
        fprintf(stderr, "source: auto -> vertex %u\n", source);
    } else {
        source = (uint32_t)strtoul(source_arg, NULL, 10);
    }

    float *dist = malloc(g.num_vertices * sizeof(float));
    // upper bound: 1 initial push + at most one push per edge relaxation
    heap_node_t *heap = malloc((g.num_edges + 1) * sizeof(heap_node_t));
    if(!dist || !heap){
        fprintf(stderr, "allocation failed\n");
        return 1;
    }

    FILE *csv_f = NULL;
    if(csv_out){
        csv_f = fopen(csv_out, "w");
        if(!csv_f) fprintf(stderr, "warning: could not open csv_out '%s' for writing\n", csv_out);
    }

    printf("run,time_ms\n");
    if(csv_f) fprintf(csv_f, "run,time_ms\n");

    int total_runs = warmup_runs + num_runs;
    for(int run = 1; run <= total_runs; run++){
        size_t heap_size;
        dijkstra_init(&g, source, dist, heap, &heap_size);

        double start = now_sec();
        dijkstra(&g, dist, heap, heap_size);
        double end = now_sec();

        if(run <= warmup_runs) continue; // discard: cache/TLB/frequency warm-up, not measured

        int measured_run = run - warmup_runs;
        double time_ms = (end - start) * 1000.0;
        printf("%d,%.6f\n", measured_run, time_ms);
        if(csv_f) fprintf(csv_f, "%d,%.6f\n", measured_run, time_ms);
    }

    if(csv_f) fclose(csv_f);

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