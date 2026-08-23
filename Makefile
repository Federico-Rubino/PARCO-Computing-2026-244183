CFLAGS = -std=c99 -O3 -Wall -march=native
COMMON = src/common/utils.c
CSR = src/common/graph_csr.c
LDFLAGS = -lm -lrt

all: dsa dijkstra

dsa:
	mkdir -p bin
	gcc $(CFLAGS) -fopenmp src/delta_stepping.c $(COMMON) $(CSR) -Isrc -o bin/dsa $(LDFLAGS)

dijkstra:
	mkdir -p bin
	gcc $(CFLAGS) src/dijkstra.c $(COMMON) $(CSR) -Isrc -o bin/dijkstra $(LDFLAGS)

clean:
	rm -rf bin/*