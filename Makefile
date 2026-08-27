CFLAGS = -std=c99 -O3 -Wall -march=native
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

clean:
	rm -rf bin/*