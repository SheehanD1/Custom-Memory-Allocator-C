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

TEST(test_malloc_write_read) {
    char *ptr = (char *)my_malloc(128);
    ASSERT(ptr != NULL, "allocation returned NULL");
    /* Write a pattern and read it back */
    for (int i = 0; i < 128; i++) {
        ptr[i] = (char)(i & 0xFF);
    }
    for (int i = 0; i < 128; i++) {
        ASSERT(ptr[i] == (char)(i & 0xFF), "data corruption detected");
    }
    my_free(ptr);
}

TEST(test_free_and_reuse) {
    /* Allocate and free, then allocate again — should reuse the block */
    void *first = my_malloc(64);
    ASSERT(first != NULL, "first allocation returned NULL");
    my_free(first);

    void *second = my_malloc(64);
    ASSERT(second != NULL, "second allocation returned NULL");
    /* The allocator should reuse the freed block */
    ASSERT(second == first, "freed block was not reused");
    my_free(second);
}

TEST(test_free_null) {
    /* free(NULL) should be a no-op, not crash */
    my_free(NULL);
}

TEST(test_calloc_zeroed) {
    unsigned char *ptr = (unsigned char *)my_calloc(10, 32);
    ASSERT(ptr != NULL, "my_calloc returned NULL");
    /* Verify every byte is zero */
    for (int i = 0; i < 320; i++) {
        ASSERT(ptr[i] == 0, "my_calloc memory not zeroed");
    }
    my_free(ptr);
}

TEST(test_calloc_overflow) {
    /* Intentionally trigger overflow: huge num * size */
    void *ptr = my_calloc((size_t)-1, 2);
    ASSERT(ptr == NULL, "my_calloc should return NULL on overflow");
}

TEST(test_realloc_grow) {
    char *ptr = (char *)my_malloc(32);
    ASSERT(ptr != NULL, "initial allocation returned NULL");
    /* Write data */
    memset(ptr, 'A', 32);

    /* Grow the allocation */
    char *new_ptr = (char *)my_realloc(ptr, 256);
    ASSERT(new_ptr != NULL, "realloc grow returned NULL");
    /* Original data should be preserved */
    for (int i = 0; i < 32; i++) {
        ASSERT(new_ptr[i] == 'A', "realloc did not preserve data");
    }
    my_free(new_ptr);
}

TEST(test_realloc_shrink) {
    char *ptr = (char *)my_malloc(256);
    ASSERT(ptr != NULL, "initial allocation returned NULL");
    memset(ptr, 'B', 256);

    /* Shrink the allocation */
    char *new_ptr = (char *)my_realloc(ptr, 32);
    ASSERT(new_ptr != NULL, "realloc shrink returned NULL");
    /* Should return the same pointer (in-place shrink) */
    ASSERT(new_ptr == ptr, "realloc shrink should be in-place");
    /* Data should be preserved in the shrunk region */
    for (int i = 0; i < 32; i++) {
        ASSERT(new_ptr[i] == 'B', "realloc shrink corrupted data");
    }
    my_free(new_ptr);
}

TEST(test_realloc_null) {
    /* realloc(NULL, size) should behave like malloc(size) */
    void *ptr = my_realloc(NULL, 64);
    ASSERT(ptr != NULL, "realloc(NULL, 64) returned NULL");
    my_free(ptr);
}

TEST(test_realloc_zero) {
    /* realloc(ptr, 0) should behave like free(ptr) */
    void *ptr = my_malloc(64);
    ASSERT(ptr != NULL, "allocation returned NULL");
    void *result = my_realloc(ptr, 0);
    ASSERT(result == NULL, "realloc(ptr, 0) should return NULL");
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
    RUN_TEST(test_malloc_write_read);
    RUN_TEST(test_free_and_reuse);
    RUN_TEST(test_free_null);
    RUN_TEST(test_calloc_zeroed);
    RUN_TEST(test_calloc_overflow);
    RUN_TEST(test_realloc_grow);
    RUN_TEST(test_realloc_shrink);
    RUN_TEST(test_realloc_null);
    RUN_TEST(test_realloc_zero);

    printf("========================================\n");
    printf("  Results: %d/%d passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
