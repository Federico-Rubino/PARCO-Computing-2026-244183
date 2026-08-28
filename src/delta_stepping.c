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

    ws->settled = (bucket_t){0};
}

static void ds_workspace_free(ds_workspace_t *ws, csr_graph_t *g){
    for(uint32_t i = 0; i < ws->num_slots; i++) free(ws->buckets[i].nodes);
    for(uint32_t i = 0; i < ws->num_slots; i++) omp_destroy_lock(&ws->bucket_locks[i]);
    free(ws->buckets);
    free(ws->bucket_locks);
    free(ws->settled.nodes);
}

static void ds_reset(ds_workspace_t *ws, csr_graph_t *g, uint32_t source, float *dist){
    for(uint64_t i = 0; i < g->num_vertices; i++) dist[i] = INFINITY;
    for(uint32_t i = 0; i < ws->num_slots; i++) ws->buckets[i].count = 0;
    dist[source] = 0.0f;
    bucket_push(&ws->buckets[0], source);
}

static void relax(float *dist, bucket_t *local_buckets, uint32_t num_slots,
                  float delta, uint32_t v, float nd){
    if(nd >= dist[v]) return;

    float old_d;
    #pragma omp atomic compare capture
    {
        old_d = dist[v];
        if(dist[v] > nd){
            dist[v] = nd;
        }
    }

    if(nd < old_d){
        // thread-local, no lock: merged into the shared buckets once per
        // thread per parallel region by merge_local_buckets()
        uint32_t slot = (uint32_t)(nd / delta) % num_slots;
        bucket_push(&local_buckets[slot], v);
    }
}

static void merge_local_buckets(bucket_t *buckets, omp_lock_t *bucket_locks,
                                 bucket_t *local_buckets, uint32_t num_slots){
    for(uint32_t s = 0; s < num_slots; s++){
        if(local_buckets[s].count == 0) continue;
        omp_set_lock(&bucket_locks[s]);
        for(uint32_t i = 0; i < local_buckets[s].count; i++)
            bucket_push(&buckets[s], local_buckets[s].nodes[i]);
        omp_unset_lock(&bucket_locks[s]);
        free(local_buckets[s].nodes);
    }
}

static void delta_stepping(csr_graph_t *g, ds_workspace_t *ws, float delta, float *dist){
    bucket_t *buckets = ws->buckets;
    omp_lock_t *bucket_locks = ws->bucket_locks;
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

            #pragma omp parallel
            {
                bucket_t local_settled = {0};
                bucket_t *local_buckets = calloc(num_slots, sizeof(bucket_t));

                #pragma omp for schedule(dynamic, 64)
                for(uint32_t i = 0; i < round_count; i++){
                    uint32_t u = round_nodes[i];

                    bucket_push(&local_settled, u);

                    for(uint64_t e = g->row_ptr[u]; e < g->row_ptr[u + 1]; e++){
                        float w = g->weights[e];
                        if(w <= delta){
                            uint32_t v = g->col_idx[e];
                            float nd = dist[u] + w;
                            relax(dist, local_buckets, num_slots, delta, v, nd);
                        }
                    }
                }

                // one lock acquisition per thread here, instead of one per vertex above
                #pragma omp critical(settled_merge)
                for(uint32_t i = 0; i < local_settled.count; i++)
                    bucket_push(&ws->settled, local_settled.nodes[i]);
                free(local_settled.nodes);

                merge_local_buckets(buckets, bucket_locks, local_buckets, num_slots);
                free(local_buckets);
            }
            free(round_nodes);
        }

        uint32_t settled_count = ws->settled.count;
        uint32_t *settled_nodes = ws->settled.nodes;

        #pragma omp parallel
        {
            bucket_t *local_buckets = calloc(num_slots, sizeof(bucket_t));

            #pragma omp for schedule(dynamic, 64)
            for(uint32_t i = 0; i < settled_count; i++){
                uint32_t u = settled_nodes[i];
                for(uint64_t e = g->row_ptr[u]; e < g->row_ptr[u + 1]; e++){
                    float w = g->weights[e];
                    if(w > delta){
                        uint32_t v = g->col_idx[e];
                        float nd = dist[u] + w;
                        relax(dist, local_buckets, num_slots, delta, v, nd);
                    }
                }
            }

            merge_local_buckets(buckets, bucket_locks, local_buckets, num_slots);
            free(local_buckets);
        }

        b++;
    }
}

int main(int argc, char **argv){
    if(argc < 2){
        fprintf(stderr, "usage: %s <graph_file> [undirected=0] [source=0] [delta=10] [warmup_runs=5] [num_runs=10] [csv_out_file] [dist_out_file]\n", argv[0]);
        return 1;
    }
    const char *filename = argv[1];
    int undirected = argc > 2 ? atoi(argv[2]) : 0;
    uint32_t source = argc > 3 ? (uint32_t)strtoul(argv[3], NULL, 10) : 0;
    float delta = argc > 4 ? (float)atof(argv[4]) : 10.0f;
    int warmup_runs = argc > 5 ? atoi(argv[5]) : 5;
    int num_runs = argc > 6 ? atoi(argv[6]) : 10;
    const char *csv_out = argc > 7 ? argv[7] : NULL;
    const char *dist_out = argc > 8 ? argv[8] : NULL;

    csr_graph_t g;
    if(load_csr(filename, &g, undirected) != 0){
        fprintf(stderr, "failed to load %s\n", filename);
        return 1;
    }

    float *dist = malloc(g.num_vertices * sizeof(float));

    ds_workspace_t ws;
    ds_workspace_init(&ws, &g, delta);

    FILE *csv_f = NULL;
    if(csv_out){
        csv_f = fopen(csv_out, "w");
        if(!csv_f) fprintf(stderr, "warning: could not open csv_out '%s' for writing\n", csv_out);
    }

    printf("run,threads,time_ms\n");
    if(csv_f) fprintf(csv_f, "run,threads,time_ms\n");

    int total_runs = warmup_runs + num_runs;
    for(int run = 1; run <= total_runs; run++){
        ds_reset(&ws, &g, source, dist);

        double start = omp_get_wtime();
        delta_stepping(&g, &ws, delta, dist);
        double end = omp_get_wtime();

        if(run <= warmup_runs) continue; // discard: thread-pool spin-up / cache warm-up, not measured

        int measured_run = run - warmup_runs;
        double time_ms = (end - start) * 1000.0;
        printf("%d,%d,%.6f\n", measured_run, omp_get_max_threads(), time_ms);
        if(csv_f) fprintf(csv_f, "%d,%d,%.6f\n", measured_run, omp_get_max_threads(), time_ms);
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

    ds_workspace_free(&ws, &g);
    free(dist);
    free_csr(&g);
    return 0;
}