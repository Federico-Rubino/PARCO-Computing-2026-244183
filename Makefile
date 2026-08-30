CFLAGS = -std=c99 -O3 -Wall -march=native
CFLAGS_PROF = -std=c99 -O3 -Wall
COMMON = src/common/utils.c
CSR = src/common/graph_csr.c
RR = src/common/graph_rr.c
LDFLAGS = -lm -lrt

all: dsa dijkstra reorder

dsa:
	mkdir -p bin
	gcc $(CFLAGS) -fopenmp src/delta_stepping.c $(COMMON) $(CSR) $(RR) -Isrc -o bin/dsa $(LDFLAGS)

dijkstra:
	mkdir -p bin
	gcc $(CFLAGS) src/dijkstra.c $(COMMON) $(CSR) $(RR) -Isrc -o bin/dijkstra $(LDFLAGS)

reorder:
	mkdir -p bin
	gcc $(CFLAGS) src/reorder_graph.c $(COMMON) $(CSR) $(RR) -Isrc -o bin/reorder $(LDFLAGS)

# no -march=native: Valgrind's instruction decoder doesn't support every
# extension a native build can emit on newer/mixed cluster hardware, and
# Cachegrind cares about memory-access patterns, not raw CPU throughput,
# so the lost vectorization doesn't affect what's being measured
dsa_prof:
	mkdir -p bin
	gcc $(CFLAGS_PROF) -fopenmp src/delta_stepping.c $(COMMON) $(CSR) $(RR) -Isrc -o bin/dsa_prof $(LDFLAGS)

dijkstra_prof:
	mkdir -p bin
	gcc $(CFLAGS_PROF) src/dijkstra.c $(COMMON) $(CSR) $(RR) -Isrc -o bin/dijkstra_prof $(LDFLAGS)

clean:
	rm -rf bin/*