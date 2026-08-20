#include "utils.h"

void addValue(int *graph, int u, int v, int w){
    graph[u*V+v] = w;
}

void generateGraph(int *graph){
    for(int u = 0; u<V; u++){
        for(int v = 0; v < V; v++){
            if(u == v){
                graph[u*V+v] = 0;
            }else{
                graph[u*V+v] = rand()%10;
            }
        }
    }
}

void initGraph(int *graph){
    for(int u = 0; u<V; u++){
        for(int v = 0; v < V; v++){
            graph[u*V+v] = INF;
        }
    }
}

void printGraph(int *graph){
    for(int u = 0; u<V; u++){
        for(int v = 0; v < V; v++){
            printf("%d ", graph[u*V+v]);
        }
        printf("\n");
    }
}