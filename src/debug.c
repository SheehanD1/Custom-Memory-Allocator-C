/*
 * debug.c — Heap Visualization and Consistency Checker
 *
 * Provides diagnostic tools for inspecting the allocator's state.
 *
 * Author: Sheehan
 */

#include "allocator.h"

#include <stdio.h>
#include <stdlib.h>

/* Access the allocator's internals (defined in allocator.c) */
extern block_header_t *free_list_head;
extern block_header_t *heap_prologue;
extern block_header_t *heap_epilogue;

/* ---------------------------------------------------------------
 * Heap Dump — Visual representation of the heap
 * --------------------------------------------------------------- */

void my_heap_dump(void) {
    if (heap_prologue == NULL) {
        printf("[heap not initialized]\n");
        return;
    }

    printf("\n");
    printf("======================================================\n");
    printf("                     HEAP DUMP                        \n");
    printf("======================================================\n");

    block_header_t *block = heap_prologue;
    int block_num = 0;

    while (block != heap_epilogue && GET_SIZE(block) > 0) {
        size_t size = GET_SIZE(block);
        int alloc = GET_ALLOC(block);
        size_t *footer = GET_FOOTER(block);

        printf("Block %-4d | Addr: %p | Size: %-8zu | %s",
               block_num,
               (void *)block,
               size,
               alloc ? "ALLOCATED" : "FREE     ");

        /* Verify footer matches header */
        if ((*footer & ~0x7) != size || ((*footer) & 0x1) != (size_t)alloc) {
            printf(" | !! FOOTER MISMATCH !!");
        }

        printf("\n");

        block = NEXT_BLOCK(block);
        block_num++;
    }

    /* Print epilogue */
    printf("Epilogue   | Addr: %p | Size: %-8zu | SENTINEL\n",
           (void *)heap_epilogue, (size_t)0);

    /* Print free list */
    printf("------------------------------------------------------\n");
    printf("Free List: ");
    block_header_t *fl = free_list_head;
    if (fl == NULL) {
        printf("(empty)");
    }
    while (fl != NULL) {
        printf("[%p size=%zu] -> ", (void *)fl, GET_SIZE(fl));
        fl = fl->next;
    }
    printf("NULL\n");
    printf("======================================================\n\n");
}

/* ---------------------------------------------------------------
 * Heap Check — Verify allocator invariants
 *
 * Returns 0 if all checks pass, non-zero on failure.
 * --------------------------------------------------------------- */

int my_heap_check(void) {
    int errors = 0;

    if (heap_prologue == NULL || heap_epilogue == NULL) {
        fprintf(stderr, "heap_check: heap not initialized\n");
        return 1;
    }

    /* --- Check 1: Header/footer consistency --- */
    block_header_t *block = heap_prologue;
    block_header_t *prev = NULL;

    while (block != heap_epilogue && GET_SIZE(block) > 0) {
        size_t size = GET_SIZE(block);
        int alloc = GET_ALLOC(block);
        size_t *footer = GET_FOOTER(block);

        /* Footer must match header */
        if ((*footer & ~0x7) != size) {
            fprintf(stderr, "heap_check: footer size mismatch at %p "
                    "(header=%zu, footer=%zu)\n",
                    (void *)block, size, *footer & ~0x7);
            errors++;
        }
        if (((*footer) & 0x1) != (size_t)alloc) {
            fprintf(stderr, "heap_check: footer alloc mismatch at %p\n",
                    (void *)block);
            errors++;
        }

        /* --- Check 2: No two adjacent free blocks --- */
        if (prev != NULL && !GET_ALLOC(prev) && !alloc) {
            fprintf(stderr, "heap_check: adjacent free blocks at %p and %p\n",
                    (void *)prev, (void *)block);
            errors++;
        }

        /* --- Check 3: Block alignment --- */
        if ((uintptr_t)PAYLOAD(block) % ALIGNMENT != 0) {
            fprintf(stderr, "heap_check: payload not aligned at %p\n",
                    (void *)block);
            errors++;
        }

        prev = block;
        block = NEXT_BLOCK(block);
    }

    /* --- Check 4: Every free block is in the free list --- */
    block = NEXT_BLOCK(heap_prologue); /* skip prologue */
    while (block != heap_epilogue && GET_SIZE(block) > 0) {
        if (!GET_ALLOC(block)) {
            /* Search for this block in the free list */
            int found = 0;
            block_header_t *fl = free_list_head;
            while (fl != NULL) {
                if (fl == block) {
                    found = 1;
                    break;
                }
                fl = fl->next;
            }
            if (!found) {
                fprintf(stderr, "heap_check: free block %p not in free list\n",
                        (void *)block);
                errors++;
            }
        }
        block = NEXT_BLOCK(block);
    }

    /* --- Check 5: Every block in the free list is actually free --- */
    block_header_t *fl = free_list_head;
    while (fl != NULL) {
        if (GET_ALLOC(fl)) {
            fprintf(stderr, "heap_check: allocated block %p in free list\n",
                    (void *)fl);
            errors++;
        }

        /* Check prev/next consistency */
        if (fl->next != NULL && fl->next->prev != fl) {
            fprintf(stderr, "heap_check: free list linkage broken at %p\n",
                    (void *)fl);
            errors++;
        }

        fl = fl->next;
    }

    return errors;
}
