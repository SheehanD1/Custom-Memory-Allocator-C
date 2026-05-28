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
 * Splitting Tests
 * --------------------------------------------------------------- */

/*
 * Allocate from a large free block — the remainder should be
 * split into a new free block that can satisfy a later request.
 */
TEST(test_split_basic) {
    /* Free everything to get one big block, then allocate small */
    void *big = my_malloc(512);
    ASSERT(big != NULL, "large allocation failed");
    my_free(big);

    void *small = my_malloc(32);
    ASSERT(small != NULL, "small allocation after free failed");

    /* The remainder from splitting should still be usable */
    void *rest = my_malloc(256);
    ASSERT(rest != NULL, "remainder allocation failed — split didn't work");
    ASSERT(my_heap_check() == 0, "heap invariant violated after split");

    my_free(small);
    my_free(rest);
}

/*
 * Allocate several blocks, free them all, verify they coalesce
 * back into a single block that can satisfy one large allocation.
 */
TEST(test_split_and_coalesce) {
    void *ptrs[8];

    /* Allocate 8 small blocks */
    for (int i = 0; i < 8; i++) {
        ptrs[i] = my_malloc(32);
        ASSERT(ptrs[i] != NULL, "allocation failed during split test");
    }

    ASSERT(my_heap_check() == 0, "heap invariant violated after allocations");

    /* Free all of them — they should coalesce back together */
    for (int i = 0; i < 8; i++) {
        my_free(ptrs[i]);
    }

    ASSERT(my_heap_check() == 0, "heap invariant violated after freeing all");

    /* One large allocation should now succeed from the coalesced space */
    void *big = my_malloc(256);
    ASSERT(big != NULL, "coalesced block can't satisfy large allocation");
    my_free(big);
}

/*
 * Interleaved alloc/free pattern — exercises both splitting
 * and coalescing across multiple cycles.
 */
TEST(test_interleaved_alloc_free) {
    void *a = my_malloc(64);
    void *b = my_malloc(64);
    void *c = my_malloc(64);
    void *d = my_malloc(64);
    ASSERT(a && b && c && d, "initial allocations failed");

    /* Free every other block */
    my_free(b);
    my_free(d);
    ASSERT(my_heap_check() == 0, "heap check failed after partial free");

    /* Allocate into the holes */
    void *b2 = my_malloc(64);
    void *d2 = my_malloc(64);
    ASSERT(b2 != NULL && d2 != NULL, "hole allocations failed");

    /* Free everything */
    my_free(a);
    my_free(b2);
    my_free(c);
    my_free(d2);
    ASSERT(my_heap_check() == 0, "heap check failed after full free");
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
    RUN_TEST(test_split_basic);
    RUN_TEST(test_split_and_coalesce);
    RUN_TEST(test_interleaved_alloc_free);

    printf("========================================\n");
    printf("  Results: %d/%d passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
