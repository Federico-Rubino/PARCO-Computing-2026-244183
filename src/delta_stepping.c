#include "common/utils.h"
#include "string.h"
#include <stdlib.h>
#include <stdio.h>

#define DELTA 2
#define NUM_BUCKETS 10

typedef struct {
    int* nodes;
    int count;
    int capacity;
} Bucket;

Bucket buckets[NUM_BUCKETS];

void delta_stepping(int* graph, int src, int* dist);
void relax(int v, int new_dist, int* dist);

int main() {
    int graphs[V*V];
    int results[V];
    int src = 0;

    // Initialize buckets
    for(int i=0; i < NUM_BUCKETS; i++) {
        buckets[i].nodes = malloc(sizeof(int) * V);
        buckets[i].count = 0;
        buckets[i].capacity = V;
    }

    initGraph(graphs);
    addValue(graphs, 0, 1, 5);
    addValue(graphs, 1, 3, 2);
    addValue(graphs, 1, 2, 1);
    addValue(graphs, 2, 4, 1);
    addValue(graphs, 4, 3, 1);

    // CRITICAL: Actually call the algorithm
    delta_stepping(graphs, src, results);

    for(int k = 0; k < V; k++){
        if(results[k] == INF) printf("from %d to %d: Unreachable\n", src, k);
        else printf("from %d to %d minimum distance %d\n", src, k, results[k]);
    }
    
    return 0;
}

void delta_stepping(int* graph, int src, int* dist) {
    for (int i = 0; i < V; i++) dist[i] = INF;
    
    // Initial relaxation
    relax(src, 0, dist);

    // Track which nodes are already in the settled list for the current bucket
    int* is_settled = malloc(sizeof(int) * V);

    for (int i = 0; i < NUM_BUCKETS; i++) {
        memset(is_settled, 0, sizeof(int) * V);
        int* settled_nodes = malloc(sizeof(int) * V);
        int settled_count = 0;

        while (buckets[i].count > 0) {
            int current_count = buckets[i].count;
            int* current_nodes = malloc(sizeof(int) * current_count);
            memcpy(current_nodes, buckets[i].nodes, sizeof(int) * current_count);
            buckets[i].count = 0; 

            for (int j = 0; j < current_count; j++) {
                int u = current_nodes[j];
                
                // Only add to settled_nodes if it's the first time this bucket sees it
                if (!is_settled[u]) {
                    settled_nodes[settled_count++] = u;
                    is_settled[u] = 1;
                }

                for (int v = 0; v < V; v++) {
                    int weight = graph[u * V + v];
                    if (weight > 0 && weight <= DELTA) {
                        relax(v, dist[u] + weight, dist);
                    }
                }
            }
            free(current_nodes);
        }

        // Phase 2: Heavy Edges
        for (int j = 0; j < settled_count; j++) {
            int u = settled_nodes[j];
            for (int v = 0; v < V; v++) {
                int weight = graph[u * V + v];
                if (weight > DELTA && weight != INF) {
                    relax(v, dist[u] + weight, dist);
                }
            }
        }
        free(settled_nodes);
    }
    free(is_settled);
}

void relax(int v, int new_dist, int* dist) {
    if (new_dist < dist[v]) {
        dist[v] = new_dist;
        int b_idx = new_dist / DELTA;
        
        if (b_idx < NUM_BUCKETS) {
            Bucket* b = &buckets[b_idx];
            b->nodes[b->count++] = v;
        }
    }
}