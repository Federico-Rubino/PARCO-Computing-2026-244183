CFLAGS = -std=c99 -O3 -Wall -march=native
COMMON = src/common/utils.c
LDFLAGS = -lm -lrt

all: dsa

dsa:
	mkdir -p bin
	gcc $(CFLAGS) src/delta_stepping.c $(COMMON) -Iinclude -o bin/dsa $(LDFLAGS)


clean:
	rm -rf bin/*