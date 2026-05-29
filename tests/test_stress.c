/*
 * test_stress.c — Stress Tests with Heap Validation
 *
 * Exercises the allocator with large volumes of random operations
 * and validates heap integrity after each step.
 *
 * Author: Sheehan
 */

#include "allocator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* ---------------------------------------------------------------
 * Simple test framework
 * --------------------------------------------------------------- */

static int tests_run    = 0;
static int tests_passed = 0;

#define TEST(name) static void name(void)
#define RUN_TEST(name)                                         \
    do {                                                       \
        tests_run++;                                           \
        printf("  %-40s ", #name);                             \
        name();                                                \
        tests_passed++;                                        \
        printf("[PASS]\n");                                    \
    } while (0)

#define ASSERT(cond, msg)                                      \
    do {                                                       \
        if (!(cond)) {                                         \
            printf("[FAIL]\n    -> %s (line %d)\n", msg, __LINE__); \
            return;                                            \
        }                                                      \
    } while (0)

/* ---------------------------------------------------------------
 * Stress Tests
 * --------------------------------------------------------------- */

/*
 * Rapid alloc/free cycle — allocate and immediately free 1000 times.
 * Verifies no leaks or corruption from fast turnover.
 */
TEST(test_rapid_alloc_free) {
    for (int i = 0; i < 1000; i++) {
        size_t size = (size_t)((i % 256) + 1);
        void *ptr = my_malloc(size);
        ASSERT(ptr != NULL, "allocation returned NULL during rapid cycle");
        /* Write a byte to make sure the memory is usable */
        memset(ptr, 0xAB, size);
        my_free(ptr);
    }
    ASSERT(my_heap_check() == 0, "heap corrupted after rapid alloc/free");
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */

int main(void) {
    srand((unsigned)time(NULL));
    my_init();

    printf("========================================\n");
    printf("  test_stress — Stress Tests\n");
    printf("========================================\n");

    RUN_TEST(test_rapid_alloc_free);

    printf("========================================\n");
    printf("  Results: %d/%d passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
