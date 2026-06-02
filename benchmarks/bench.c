/*
 * bench.c — Benchmarking Harness
 *
 * Measures allocator throughput and compares against
 * the system malloc implementation.
 *
 * Author: Sheehan
 */

#include "allocator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---------------------------------------------------------------
 * Timing Helpers
 * --------------------------------------------------------------- */

/* Returns elapsed time in seconds between two timespecs */
static double elapsed_sec(struct timespec start, struct timespec end) {
    double s = (double)(end.tv_sec - start.tv_sec);
    double ns = (double)(end.tv_nsec - start.tv_nsec);
    return s + ns / 1e9;
}

/* ---------------------------------------------------------------
 * Throughput Benchmark
 *
 * Time N allocations + frees, report operations per second.
 * --------------------------------------------------------------- */

#define BENCH_OPS 10000
#define BENCH_SIZE 64

static void bench_throughput_custom(void) {
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < BENCH_OPS; i++) {
        void *ptr = my_malloc(BENCH_SIZE);
        if (ptr) my_free(ptr);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double secs = elapsed_sec(start, end);
    double ops_per_sec = (BENCH_OPS * 2.0) / secs; /* alloc + free = 2 ops */

    printf("  Custom allocator:  %10.0f ops/sec  (%.4f sec for %d cycles)\n",
           ops_per_sec, secs, BENCH_OPS);
}

static void bench_throughput_system(void) {
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < BENCH_OPS; i++) {
        void *ptr = malloc(BENCH_SIZE);
        if (ptr) free(ptr);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double secs = elapsed_sec(start, end);
    double ops_per_sec = (BENCH_OPS * 2.0) / secs;

    printf("  System malloc:     %10.0f ops/sec  (%.4f sec for %d cycles)\n",
           ops_per_sec, secs, BENCH_OPS);
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */

int main(void) {
    my_init();

    printf("========================================\n");
    printf("  Allocator Benchmarks\n");
    printf("========================================\n\n");

    printf("--- Throughput (alloc+free, %d-byte blocks) ---\n", BENCH_SIZE);
    bench_throughput_custom();
    bench_throughput_system();

    printf("\n========================================\n");
    printf("  Benchmarks complete.\n");
    printf("========================================\n");

    return 0;
}
