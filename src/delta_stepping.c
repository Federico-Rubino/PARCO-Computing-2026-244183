#include "common/graph_csr.h"
#include <omp.h>
#include <math.h>
#include <inttypes.h>

typedef struct {
    uint32_t *nodes;
    uint32_t count;
    uint32_t capacity;
} bucket_t;

static void bucket_push(bucket_t *b, uint32_t v){
    if(b->count == b->capacity){
        b->capacity = b->capacity ? b->capacity * 2 : 16;
        b->nodes = realloc(b->nodes, b->capacity * sizeof(uint32_t));
    }
    b->nodes[b->count++] = v;
}

typedef struct {
    bucket_t *buckets;
    omp_lock_t *bucket_locks;
    omp_lock_t *vertex_locks;
    bucket_t settled;
    uint32_t num_slots;
} ds_workspace_t;

static void ds_workspace_init(ds_workspace_t *ws, csr_graph_t *g, float delta){
    float max_w = 0.0f;
    for(uint64_t i = 0; i < g->num_edges; i++)
        if(g->weights[i] > max_w) max_w = g->weights[i];

    // a vertex can be relaxed at most ceil(max_w/delta) buckets ahead of the
    // one currently being processed; +2 gives margin against edge rounding
    ws->num_slots = (uint32_t)ceilf(max_w / delta) + 2;

    ws->buckets = calloc(ws->num_slots, sizeof(bucket_t));
    ws->bucket_locks = malloc(ws->num_slots * sizeof(omp_lock_t));
    for(uint32_t i = 0; i < ws->num_slots; i++) omp_init_lock(&ws->bucket_locks[i]);

    ws->vertex_locks = malloc(g->num_vertices * sizeof(omp_lock_t));
    for(uint64_t i = 0; i < g->num_vertices; i++) omp_init_lock(&ws->vertex_locks[i]);

    ws->settled = (bucket_t){0};
}

static void ds_workspace_free(ds_workspace_t *ws, csr_graph_t *g){
    for(uint32_t i = 0; i < ws->num_slots; i++) free(ws->buckets[i].nodes);
    for(uint32_t i = 0; i < ws->num_slots; i++) omp_destroy_lock(&ws->bucket_locks[i]);
    for(uint64_t i = 0; i < g->num_vertices; i++) omp_destroy_lock(&ws->vertex_locks[i]);
    free(ws->buckets);
    free(ws->bucket_locks);
    free(ws->vertex_locks);
    free(ws->settled.nodes);
}

static void ds_reset(ds_workspace_t *ws, csr_graph_t *g, uint32_t source, float *dist){
    for(uint64_t i = 0; i < g->num_vertices; i++) dist[i] = INFINITY;
    for(uint32_t i = 0; i < ws->num_slots; i++) ws->buckets[i].count = 0;
    dist[source] = 0.0f;
    bucket_push(&ws->buckets[0], source);
}

static void relax(float *dist, omp_lock_t *vertex_locks, bucket_t *buckets,
                   omp_lock_t *bucket_locks, uint32_t num_slots, float delta,
                   uint32_t v, float nd){
    omp_set_lock(&vertex_locks[v]);
    int improved = nd < dist[v];
    if(improved) dist[v] = nd;
    omp_unset_lock(&vertex_locks[v]);

    if(improved){
        uint32_t slot = (uint32_t)(nd / delta) % num_slots;
        omp_set_lock(&bucket_locks[slot]);
        bucket_push(&buckets[slot], v);
        omp_unset_lock(&bucket_locks[slot]);
    }
}

static void delta_stepping(csr_graph_t *g, ds_workspace_t *ws, float delta, float *dist){
    bucket_t *buckets = ws->buckets;
    omp_lock_t *bucket_locks = ws->bucket_locks;
    omp_lock_t *vertex_locks = ws->vertex_locks;
    uint32_t num_slots = ws->num_slots;

    uint32_t consecutive_empty = 0;
    uint64_t b = 0;

    while(consecutive_empty < num_slots){
        uint32_t slot = (uint32_t)(b % num_slots);
        if(buckets[slot].count == 0){
            consecutive_empty++;
            b++;
            continue;
        }
        consecutive_empty = 0;
        ws->settled.count = 0;

        while(buckets[slot].count > 0){
            uint32_t round_count = buckets[slot].count;
            uint32_t *round_nodes = buckets[slot].nodes;
            buckets[slot].nodes = NULL;
            buckets[slot].count = 0;
            buckets[slot].capacity = 0;

            #pragma omp parallel for schedule(dynamic, 64)
            for(uint32_t i = 0; i < round_count; i++){
                uint32_t u = round_nodes[i];

                #pragma omp critical(settled_merge)
                bucket_push(&ws->settled, u);

                for(uint64_t e = g->row_ptr[u]; e < g->row_ptr[u + 1]; e++){
                    float w = g->weights[e];
                    if(w <= delta){
                        uint32_t v = g->col_idx[e];
                        float nd = dist[u] + w;
                        relax(dist, vertex_locks, buckets, bucket_locks, num_slots, delta, v, nd);
                    }
                }
            }
            free(round_nodes);
        }

        uint32_t settled_count = ws->settled.count;
        uint32_t *settled_nodes = ws->settled.nodes;

        #pragma omp parallel for schedule(dynamic, 64)
        for(uint32_t i = 0; i < settled_count; i++){
            uint32_t u = settled_nodes[i];
            for(uint64_t e = g->row_ptr[u]; e < g->row_ptr[u + 1]; e++){
                float w = g->weights[e];
                if(w > delta){
                    uint32_t v = g->col_idx[e];
                    float nd = dist[u] + w;
                    relax(dist, vertex_locks, buckets, bucket_locks, num_slots, delta, v, nd);
                }
            }
        }

        b++;
    }
}

int main(int argc, char **argv){
    if(argc < 2){
        fprintf(stderr, "usage: %s <graph_file> [undirected=0] [source=0] [delta=10] [num_runs=1] [dist_out_file]\n", argv[0]);
        return 1;
    }
    const char *filename = argv[1];
    int undirected = argc > 2 ? atoi(argv[2]) : 0;
    uint32_t source = argc > 3 ? (uint32_t)strtoul(argv[3], NULL, 10) : 0;
    float delta = argc > 4 ? (float)atof(argv[4]) : 10.0f;
    int num_runs = argc > 5 ? atoi(argv[5]) : 1;
    const char *dist_out = argc > 6 ? argv[6] : NULL;

    csr_graph_t g;
    if(load_csr(filename, &g, undirected) != 0){
        fprintf(stderr, "failed to load %s\n", filename);
        return 1;
    }

    float *dist = malloc(g.num_vertices * sizeof(float));

    ds_workspace_t ws;
    ds_workspace_init(&ws, &g, delta);

    printf("run,threads,time_ms\n");
    for(int run = 1; run <= num_runs; run++){
        ds_reset(&ws, &g, source, dist);

        double start = omp_get_wtime();
        delta_stepping(&g, &ws, delta, dist);
        double end = omp_get_wtime();

        printf("%d,%d,%.6f\n", run, omp_get_max_threads(), (end - start) * 1000.0);
    }

    if(dist_out){
        FILE *f = fopen(dist_out, "w");
        if(f){
            for(uint64_t v = 0; v < g.num_vertices; v++)
                fprintf(f, "%" PRIu64 " %f\n", v, dist[v]);
            fclose(f);
        }
    }

    ds_workspace_free(&ws, &g);
    free(dist);
    free_csr(&g);
    return 0;
}
