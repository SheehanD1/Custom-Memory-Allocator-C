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

#define POOL_SIZE 64
#define NUM_OPS  500
#define CHECK_INTERVAL 50

/*
 * Random alloc/free pattern — maintain a pool of live pointers,
 * randomly choose to allocate or free on each iteration.
 * Validates heap every CHECK_INTERVAL operations.
 */
TEST(test_random_alloc_free) {
    void  *pool[POOL_SIZE];
    size_t sizes[POOL_SIZE];
    int    count = 0;

    memset(pool, 0, sizeof(pool));
    memset(sizes, 0, sizeof(sizes));

    for (int op = 0; op < NUM_OPS; op++) {
        int do_alloc = (count == 0) || (count < POOL_SIZE && rand() % 2 == 0);

        if (do_alloc) {
            /* Allocate a random size between 1 and 512 bytes */
            size_t size = (size_t)(rand() % 512) + 1;

            /* Find an empty slot */
            int slot = -1;
            for (int i = 0; i < POOL_SIZE; i++) {
                if (pool[i] == NULL) {
                    slot = i;
                    break;
                }
            }
            ASSERT(slot >= 0, "no empty slot in pool");

            pool[slot] = my_malloc(size);
            ASSERT(pool[slot] != NULL, "allocation failed during random test");

            /* Write a pattern so we can detect corruption later */
            memset(pool[slot], (unsigned char)(slot & 0xFF), size);
            sizes[slot] = size;
            count++;
        } else {
            /* Free a random live pointer */
            int slot = rand() % POOL_SIZE;
            /* Find the next occupied slot */
            for (int i = 0; i < POOL_SIZE; i++) {
                int idx = (slot + i) % POOL_SIZE;
                if (pool[idx] != NULL) {
                    slot = idx;
                    break;
                }
            }

            if (pool[slot] != NULL) {
                /* Verify the pattern is still intact before freeing */
                unsigned char expected = (unsigned char)(slot & 0xFF);
                unsigned char *bytes = (unsigned char *)pool[slot];
                for (size_t j = 0; j < sizes[slot]; j++) {
                    ASSERT(bytes[j] == expected, "data corruption detected before free");
                }

                my_free(pool[slot]);
                pool[slot] = NULL;
                sizes[slot] = 0;
                count--;
            }
        }

        /* Periodic heap validation */
        if (op % CHECK_INTERVAL == 0) {
            ASSERT(my_heap_check() == 0, "heap corrupted during random ops");
        }
    }

    /* Clean up remaining allocations */
    for (int i = 0; i < POOL_SIZE; i++) {
        if (pool[i] != NULL) {
            my_free(pool[i]);
        }
    }
    ASSERT(my_heap_check() == 0, "heap corrupted after random test cleanup");
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
    RUN_TEST(test_random_alloc_free);

    printf("========================================\n");
    printf("  Results: %d/%d passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
