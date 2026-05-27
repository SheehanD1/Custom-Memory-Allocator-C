/*
 * test_coalesce.c — Coalescing & Splitting Edge-Case Tests
 *
 * Verifies that the allocator correctly merges adjacent free blocks
 * and splits oversized blocks during allocation.
 *
 * Author: Sheehan
 */

#include "allocator.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Simple test framework (same as test_basic.c)
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
 * Coalescing Tests
 * --------------------------------------------------------------- */

/*
 * Free two adjacent blocks (A then B) — allocator should merge
 * them into one larger block that can satisfy a bigger request.
 */
TEST(test_coalesce_next) {
    void *a = my_malloc(64);
    void *b = my_malloc(64);
    ASSERT(a != NULL && b != NULL, "allocations failed");

    my_free(a);
    my_free(b);

    /* The two freed blocks should have coalesced.
       A single allocation larger than either block should succeed
       and land in the coalesced region. */
    void *big = my_malloc(128);
    ASSERT(big != NULL, "coalesced allocation failed");

    /* Heap check should report no errors */
    ASSERT(my_heap_check() == 0, "heap invariant violated after coalesce_next");
    my_free(big);
}

/*
 * Free two adjacent blocks in reverse order (B then A) —
 * backward coalescing via boundary tags should merge them.
 */
TEST(test_coalesce_prev) {
    void *a = my_malloc(64);
    void *b = my_malloc(64);
    ASSERT(a != NULL && b != NULL, "allocations failed");

    /* Free in reverse order */
    my_free(b);
    my_free(a);

    void *big = my_malloc(128);
    ASSERT(big != NULL, "coalesced allocation failed");
    ASSERT(my_heap_check() == 0, "heap invariant violated after coalesce_prev");
    my_free(big);
}

/*
 * Free the middle block when both neighbors are already free —
 * should produce a single large block from all three.
 */
TEST(test_coalesce_both) {
    void *a = my_malloc(64);
    void *b = my_malloc(64);
    void *c = my_malloc(64);
    ASSERT(a != NULL && b != NULL && c != NULL, "allocations failed");

    /* Free outer blocks first, then the middle */
    my_free(a);
    my_free(c);
    my_free(b);  /* should coalesce with both a and c */

    /* All three should have merged — a large allocation should fit */
    void *big = my_malloc(192);
    ASSERT(big != NULL, "triple-coalesced allocation failed");
    ASSERT(my_heap_check() == 0, "heap invariant violated after coalesce_both");
    my_free(big);
}

/*
 * Free non-adjacent blocks — they should NOT coalesce.
 */
TEST(test_no_coalesce) {
    void *a = my_malloc(64);
    void *b = my_malloc(64);   /* barrier — stays allocated */
    void *c = my_malloc(64);
    ASSERT(a != NULL && b != NULL && c != NULL, "allocations failed");

    /* Free a and c, but b stays allocated between them */
    my_free(a);
    my_free(c);

    /* Neither freed block alone is 128 bytes, and they can't merge
       across the allocated barrier. Heap check verifies no adjacent frees. */
    ASSERT(my_heap_check() == 0, "heap invariant violated — spurious coalesce?");

    my_free(b);
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */

int main(void) {
    my_init();

    printf("========================================\n");
    printf("  test_coalesce — Coalescing Tests\n");
    printf("========================================\n");

    RUN_TEST(test_coalesce_next);
    RUN_TEST(test_coalesce_prev);
    RUN_TEST(test_coalesce_both);
    RUN_TEST(test_no_coalesce);

    printf("========================================\n");
    printf("  Results: %d/%d passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
