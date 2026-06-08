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
 * Fragmentation Benchmark
 *
 * Allocate a mix of sizes, free half, allocate again.
 * Measure utilization = (active payload) / (total heap growth).
 * --------------------------------------------------------------- */

#define FRAG_COUNT 200

static void bench_fragmentation(void) {
    void  *ptrs[FRAG_COUNT];
    size_t sizes[FRAG_COUNT];
    size_t total_payload = 0;

    srand(42); /* deterministic for reproducibility */

    /* Phase 1: allocate FRAG_COUNT blocks of random sizes */
    for (int i = 0; i < FRAG_COUNT; i++) {
        sizes[i] = (size_t)(rand() % 256) + 8;
        ptrs[i] = my_malloc(sizes[i]);
        total_payload += sizes[i];
    }

    /* Phase 2: free every other block (creates fragmentation) */
    size_t freed_payload = 0;
    for (int i = 0; i < FRAG_COUNT; i += 2) {
        my_free(ptrs[i]);
        freed_payload += sizes[i];
        ptrs[i] = NULL;
    }

    size_t active_payload = total_payload - freed_payload;

    /* Phase 3: try to allocate into the holes */
    int reuse_count = 0;
    for (int i = 0; i < FRAG_COUNT; i += 2) {
        sizes[i] = (size_t)(rand() % 128) + 8;
        ptrs[i] = my_malloc(sizes[i]);
        if (ptrs[i] != NULL) {
            reuse_count++;
            active_payload += sizes[i];
        }
    }

    /* Report */
    void *brk = sbrk(0);
    printf("  Active payload:    %zu bytes\n", active_payload);
    printf("  Current brk:       %p\n", brk);
    printf("  Hole reuse:        %d / %d freed slots refilled\n",
           reuse_count, FRAG_COUNT / 2);

    /* Cleanup */
    for (int i = 0; i < FRAG_COUNT; i++) {
        if (ptrs[i] != NULL) my_free(ptrs[i]);
    }
}

/* ---------------------------------------------------------------
 * Mixed-Workload Throughput
 *
 * Random allocation sizes with interleaved frees — more realistic
 * than the fixed-size throughput benchmark.
 * --------------------------------------------------------------- */

#define MIX_OPS  5000
#define MIX_POOL 128

static void bench_mixed_workload(void) {
    void *pool[MIX_POOL];
    int count = 0;
    struct timespec start, end;

    memset(pool, 0, sizeof(pool));
    srand(123);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int op = 0; op < MIX_OPS; op++) {
        if (count == 0 || (count < MIX_POOL && rand() % 3 != 0)) {
            /* Allocate — 2/3 chance when pool isn't full */
            int slot = -1;
            for (int i = 0; i < MIX_POOL; i++) {
                if (pool[i] == NULL) { slot = i; break; }
            }
            if (slot >= 0) {
                size_t size = (size_t)(rand() % 512) + 1;
                pool[slot] = my_malloc(size);
                if (pool[slot]) count++;
            }
        } else {
            /* Free a random occupied slot */
            int slot = rand() % MIX_POOL;
            for (int i = 0; i < MIX_POOL; i++) {
                int idx = (slot + i) % MIX_POOL;
                if (pool[idx] != NULL) {
                    my_free(pool[idx]);
                    pool[idx] = NULL;
                    count--;
                    break;
                }
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    /* Cleanup remaining */
    for (int i = 0; i < MIX_POOL; i++) {
        if (pool[i] != NULL) my_free(pool[i]);
    }

    double secs = elapsed_sec(start, end);
    double ops_per_sec = (double)MIX_OPS / secs;

    printf("  Custom allocator:  %10.0f ops/sec  (%.4f sec for %d ops)\n",
           ops_per_sec, secs, MIX_OPS);
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

    printf("\n--- Fragmentation (mixed sizes, free half, refill) ---\n");
    bench_fragmentation();

    printf("\n--- Mixed Workload (random sizes, interleaved) ---\n");
    bench_mixed_workload();

    printf("\n========================================\n");
    printf("  Benchmarks complete.\n");
    printf("========================================\n");

    return 0;
}
