#include "common/graph_csr.h"
#include "common/graph_rr.h"
#include <omp.h>
#include <math.h>
#include <inttypes.h>
#include <string.h>

#define WASP_IDLE INT32_MAX

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

// mutex-protected current bucket, steals only when fully idle
typedef struct {
    bucket_t current;    // stealable: vertices at curr_prio
    bucket_t stage;      // same-priority output, merged into current after a round
    omp_lock_t lock;     // protects current
    int curr_prio;       // WASP_IDLE when this thread has no work
    bucket_t *local;     // local[p]: vertices at priority p, thread-private
    int local_cap;
} wasp_thread_t;

typedef struct {
    wasp_thread_t *threads;
    int num_threads;
    int active_count;
} wasp_workspace_t;

static void wasp_workspace_init(wasp_workspace_t *ws){
    ws->num_threads = omp_get_max_threads();
    ws->threads = calloc(ws->num_threads, sizeof(wasp_thread_t));
    for(int t = 0; t < ws->num_threads; t++)
        omp_init_lock(&ws->threads[t].lock);
}

static void wasp_workspace_free(wasp_workspace_t *ws){
    for(int t = 0; t < ws->num_threads; t++){
        wasp_thread_t *th = &ws->threads[t];
        free(th->current.nodes);
        free(th->stage.nodes);
        for(int p = 0; p < th->local_cap; p++) free(th->local[p].nodes);
        free(th->local);
        omp_destroy_lock(&th->lock);
    }
    free(ws->threads);
}

static void wasp_reset(wasp_workspace_t *ws, csr_graph_t *g, uint32_t source, float *dist){
    for(uint64_t i = 0; i < g->num_vertices; i++) dist[i] = INFINITY;
    dist[source] = 0.0f;

    for(int t = 0; t < ws->num_threads; t++){
        wasp_thread_t *th = &ws->threads[t];
        th->current.count = 0;
        th->stage.count = 0;
        for(int p = 0; p < th->local_cap; p++) th->local[p].count = 0;
        th->curr_prio = WASP_IDLE;
    }

    bucket_push(&ws->threads[0].current, source);
    ws->threads[0].curr_prio = 0;
    ws->active_count = ws->num_threads;
}

static void wasp_grow_local(wasp_thread_t *th, int p){
    if(p < th->local_cap) return;
    int new_cap = th->local_cap ? th->local_cap : 1;
    while(new_cap <= p) new_cap *= 2;
    th->local = realloc(th->local, new_cap * sizeof(bucket_t));
    for(int i = th->local_cap; i < new_cap; i++) th->local[i] = (bucket_t){0};
    th->local_cap = new_cap;
}

static void wasp_relax(float *dist, wasp_thread_t *self, int curr_prio, float delta, uint32_t v, float nd){
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
        int p = (int)(nd / delta);
        if(p == curr_prio){
            // same priority as current round: buffer, merged in after
            bucket_push(&self->stage, v);
        } else {
            wasp_grow_local(self, p);
            bucket_push(&self->local[p], v);
        }
    }
}

// -1 if the thread has no pending local work
static int wasp_find_next_local(wasp_thread_t *th){
    for(int p = 0; p < th->local_cap; p++)
        if(th->local[p].count > 0) return p;
    return -1;
}

// steals half of a victim's current bucket, round robin over peers
static int wasp_try_steal(wasp_thread_t *threads, int nthreads, int self,
                           bucket_t *out, int *out_prio){
    for(int off = 1; off < nthreads; off++){
        int v = (self + off) % nthreads;
        int prio_v;
        #pragma omp atomic read
        prio_v = threads[v].curr_prio;
        if(prio_v == WASP_IDLE) continue;
        if(!omp_test_lock(&threads[v].lock)) continue;

        uint32_t n = threads[v].current.count;
        if(n < 2){
            omp_unset_lock(&threads[v].lock);
            continue;
        }
        uint32_t steal_n = n / 2;
        for(uint32_t i = 0; i < steal_n; i++) bucket_push(out, threads[v].current.nodes[i]);
        uint32_t remaining = n - steal_n;
        memmove(threads[v].current.nodes, threads[v].current.nodes + steal_n, remaining * sizeof(uint32_t));
        threads[v].current.count = remaining;
        *out_prio = threads[v].curr_prio;
        omp_unset_lock(&threads[v].lock);
        return 1;
    }
    return 0;
}

static void wasp_worker(int tid, wasp_workspace_t *ws, csr_graph_t *g, float *dist, float delta){
    wasp_thread_t *threads = ws->threads;
    wasp_thread_t *self = &threads[tid];
    int nthreads = ws->num_threads;
    bucket_t stolen;
    int stolen_prio;

    for(;;){
        if(self->current.count == 0){
            int p = wasp_find_next_local(self);
            if(p >= 0){
                omp_set_lock(&self->lock);
                bucket_t tmp = self->current;
                self->current = self->local[p];
                self->local[p] = tmp;
                self->local[p].count = 0;
                #pragma omp atomic write
                self->curr_prio = p;
                omp_unset_lock(&self->lock);
            } else {
                stolen = (bucket_t){0};
                if(wasp_try_steal(threads, nthreads, tid, &stolen, &stolen_prio)){
                    omp_set_lock(&self->lock);
                    for(uint32_t i = 0; i < stolen.count; i++) bucket_push(&self->current, stolen.nodes[i]);
                    #pragma omp atomic write
                    self->curr_prio = stolen_prio;
                    omp_unset_lock(&self->lock);
                    free(stolen.nodes);
                } else {
                    // genuinely idle: no local work, no steal found anywhere
                    #pragma omp atomic write
                    self->curr_prio = WASP_IDLE;
                    int active;
                    #pragma omp atomic capture
                    { ws->active_count--; active = ws->active_count; }
                    if(active == 0) return;

                    for(;;){
                        #pragma omp atomic read
                        active = ws->active_count;
                        if(active == 0) return;

                        stolen = (bucket_t){0};
                        if(wasp_try_steal(threads, nthreads, tid, &stolen, &stolen_prio)){
                            #pragma omp atomic update
                            ws->active_count++;
                            omp_set_lock(&self->lock);
                            for(uint32_t i = 0; i < stolen.count; i++) bucket_push(&self->current, stolen.nodes[i]);
                            #pragma omp atomic write
                            self->curr_prio = stolen_prio;
                            omp_unset_lock(&self->lock);
                            free(stolen.nodes);
                            break;
                        }
                    }
                }
                continue;
            }
        }

        omp_set_lock(&self->lock);
        bucket_t round = self->current;
        self->current = (bucket_t){0};
        omp_unset_lock(&self->lock);

        int prio = self->curr_prio;
        self->stage.count = 0;

        for(uint32_t i = 0; i < round.count; i++){
            uint32_t u = round.nodes[i];
            if(dist[u] < prio * delta) continue; // stale: a better path already moved u to an earlier bucket

            for(uint64_t e = g->row_ptr[u]; e < g->row_ptr[u + 1]; e++){
                uint32_t v = g->col_idx[e];
                float nd = dist[u] + g->weights[e];
                wasp_relax(dist, self, prio, delta, v, nd);
            }
        }
        free(round.nodes);

        if(self->stage.count > 0){
            omp_set_lock(&self->lock);
            for(uint32_t i = 0; i < self->stage.count; i++) bucket_push(&self->current, self->stage.nodes[i]);
            omp_unset_lock(&self->lock);
        }
    }
}

static void wasp_sssp(csr_graph_t *g, wasp_workspace_t *ws, float delta, float *dist){
    int nthreads = ws->num_threads;
    #pragma omp parallel num_threads(nthreads)
    {
        int tid = omp_get_thread_num();
        wasp_worker(tid, ws, g, dist, delta);
    }
}

int main(int argc, char **argv){
    if(argc < 2){
        fprintf(stderr, "usage: %s <graph_file> [undirected=0] [source=auto] [delta=10] [warmup_runs=5] [num_runs=10] [csv_out_file] [dist_out_file]\n", argv[0]);
        return 1;
    }
    const char *filename = argv[1];
    int undirected = argc > 2 ? atoi(argv[2]) : 0;
    const char *source_arg = argc > 3 ? argv[3] : "auto";
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

    const char *rr_env = getenv("RABBIT_REORDER");
    if(!(rr_env && strcmp(rr_env, "0") == 0)){
        csr_graph_t reordered;
        if(csr_rabbit_order(&g, &reordered, NULL, NULL) == 0){
            free_csr(&g);
            g = reordered;
        } else {
            fprintf(stderr, "warning: rabbit reordering failed, using original order\n");
        }
    }

    uint32_t source;
    if(strcmp(source_arg, "auto") == 0){
        source = max_out_degree_vertex(&g);
        fprintf(stderr, "source: auto -> vertex %u\n", source);
    } else {
        source = (uint32_t)strtoul(source_arg, NULL, 10);
    }

    float *dist = malloc(g.num_vertices * sizeof(float));

    wasp_workspace_t ws;
    wasp_workspace_init(&ws);

    FILE *csv_f = NULL;
    if(csv_out){
        csv_f = fopen(csv_out, "w");
        if(!csv_f) fprintf(stderr, "warning: could not open csv_out '%s' for writing\n", csv_out);
    }

    printf("run,threads,time_ms\n");
    if(csv_f) fprintf(csv_f, "run,threads,time_ms\n");

    int total_runs = warmup_runs + num_runs;
    for(int run = 1; run <= total_runs; run++){
        wasp_reset(&ws, &g, source, dist);

        double start = omp_get_wtime();
        wasp_sssp(&g, &ws, delta, dist);
        double end = omp_get_wtime();

        if(run <= warmup_runs) continue; // discard: thread-pool spin-up / cache warm-up, not measured

        int measured_run = run - warmup_runs;
        double time_ms = (end - start) * 1000.0;
        printf("%d,%d,%.6f\n", measured_run, ws.num_threads, time_ms);
        if(csv_f) fprintf(csv_f, "%d,%d,%.6f\n", measured_run, ws.num_threads, time_ms);
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

    wasp_workspace_free(&ws);
    free(dist);
    free_csr(&g);
    return 0;
}
