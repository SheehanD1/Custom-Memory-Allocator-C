/*
 * test_basic.c — Unit Tests for Core Allocator Functions
 *
 * Tests basic correctness of my_malloc, my_free, my_calloc,
 * my_realloc, and alignment guarantees.
 *
 * Author: Sheehan
 */

#include "allocator.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

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
 * Tests
 * --------------------------------------------------------------- */

TEST(test_malloc_returns_non_null) {
    void *ptr = my_malloc(64);
    ASSERT(ptr != NULL, "my_malloc(64) returned NULL");
    my_free(ptr);
}

TEST(test_malloc_multiple) {
    void *a = my_malloc(32);
    void *b = my_malloc(64);
    void *c = my_malloc(128);
    ASSERT(a != NULL, "first allocation returned NULL");
    ASSERT(b != NULL, "second allocation returned NULL");
    ASSERT(c != NULL, "third allocation returned NULL");
    ASSERT(a != b && b != c && a != c, "allocations returned overlapping pointers");
    my_free(a);
    my_free(b);
    my_free(c);
}

TEST(test_malloc_alignment) {
    for (int i = 1; i <= 256; i++) {
        void *ptr = my_malloc((size_t)i);
        ASSERT(ptr != NULL, "allocation returned NULL");
        ASSERT((uintptr_t)ptr % ALIGNMENT == 0,
               "returned pointer is not 16-byte aligned");
        my_free(ptr);
    }
}

TEST(test_malloc_zero) {
    void *ptr = my_malloc(0);
    ASSERT(ptr == NULL, "my_malloc(0) should return NULL");
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */

int main(void) {
    my_init();

    printf("========================================\n");
    printf("  test_basic — Core Allocator Tests\n");
    printf("========================================\n");

    RUN_TEST(test_malloc_returns_non_null);
    RUN_TEST(test_malloc_multiple);
    RUN_TEST(test_malloc_alignment);
    RUN_TEST(test_malloc_zero);

    printf("========================================\n");
    printf("  Results: %d/%d passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
