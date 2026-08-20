#ifndef __UTILS_H__
#define __UTILS_H__

#include "stdlib.h"
#include "stdio.h"
#include "time.h"
#include "limits.h"
#include "stdint.h"

#define V 5
#define INF (INT_MAX/2)

void addValue(int *graph, int u, int v, int w);

void generateGraph(int *graph);

void initGraph(int *graph);

void printGraph(int *graph);

#endif