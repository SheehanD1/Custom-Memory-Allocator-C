/*
 * allocator.c — Custom Memory Allocator Implementation
 *
 * Explicit free list with boundary-tag coalescing, block splitting,
 * and configurable first-fit / best-fit search strategy.
 *
 * Author: Sheehan
 */

#include "allocator.h"

#include <unistd.h>   /* sbrk */
#include <string.h>   /* memcpy, memset */
#include <stdio.h>    /* fprintf (debug) */
#include <stdint.h>

/* ---------------------------------------------------------------
 * Globals
 * --------------------------------------------------------------- */

/* Head of the explicit free list (doubly-linked, LIFO insertion) */
static block_header_t *free_list_head = NULL;

/* Pointer to the prologue block (sentinel at heap start) */
static block_header_t *heap_prologue = NULL;

/* Pointer to the epilogue header (sentinel at heap end) */
static block_header_t *heap_epilogue = NULL;

/* Track whether the allocator has been initialized */
static int initialized = 0;

/* ---------------------------------------------------------------
 * Internal: Free List Operations
 * --------------------------------------------------------------- */

/*
 * Insert a free block at the head of the explicit free list (LIFO).
 */
static void free_list_insert(block_header_t *block) {
    block->next = free_list_head;
    block->prev = NULL;
    if (free_list_head != NULL) {
        free_list_head->prev = block;
    }
    free_list_head = block;
}

/*
 * Remove a block from the explicit free list.
 */
static void free_list_remove(block_header_t *block) {
    if (block->prev != NULL) {
        block->prev->next = block->next;
    } else {
        free_list_head = block->next;
    }
    if (block->next != NULL) {
        block->next->prev = block->prev;
    }
    block->next = NULL;
    block->prev = NULL;
}

/* ---------------------------------------------------------------
 * Internal: Block Helpers
 * --------------------------------------------------------------- */

/*
 * Write header and footer for a block with the given size and alloc bit.
 */
static void write_block(block_header_t *block, size_t size, int alloc) {
    block->size = PACK(size, alloc);
    size_t *footer = GET_FOOTER(block);
    *footer = PACK(size, alloc);
}

/*
 * Calculate the minimum block size needed for a given payload.
 * Ensures alignment and minimum size constraints.
 */
static size_t adjust_size(size_t payload_size) {
    size_t size;

    if (payload_size == 0) {
        return ALIGN(OVERHEAD + MIN_BLOCK_SIZE, ALIGNMENT);
    }

    /* Ensure the block is large enough for the free list pointers */
    if (payload_size < MIN_BLOCK_SIZE) {
        payload_size = MIN_BLOCK_SIZE;
    }

    size = ALIGN(payload_size + OVERHEAD, ALIGNMENT);
    return size;
}

/* ---------------------------------------------------------------
 * Internal: Search Strategies
 * --------------------------------------------------------------- */

/*
 * First-fit: return the first free block that is large enough.
 */
static block_header_t *find_first_fit(size_t size) {
    block_header_t *current = free_list_head;

    while (current != NULL) {
        if (GET_SIZE(current) >= size) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/*
 * Best-fit: return the smallest free block that is large enough.
 */
static block_header_t *find_best_fit(size_t size) {
    block_header_t *current = free_list_head;
    block_header_t *best = NULL;

    while (current != NULL) {
        size_t block_size = GET_SIZE(current);
        if (block_size >= size) {
            if (best == NULL || block_size < GET_SIZE(best)) {
                best = current;
                /* Exact fit — can't do better */
                if (block_size == size) {
                    return best;
                }
            }
        }
        current = current->next;
    }
    return best;
}

/*
 * Find a suitable free block using the configured strategy.
 */
static block_header_t *find_fit(size_t size) {
    if (STRATEGY_BEST_FIT) {
        return find_best_fit(size);
    }
    return find_first_fit(size);
}

/* ---------------------------------------------------------------
 * Internal: Splitting
 * --------------------------------------------------------------- */

/*
 * If the block is large enough, split it:
 *   - The first part becomes the allocated block of `size` bytes.
 *   - The remainder becomes a new free block inserted into the free list.
 */
static void split_block(block_header_t *block, size_t size) {
    size_t block_size = GET_SIZE(block);
    size_t remainder = block_size - size;

    if (remainder >= ALIGN(OVERHEAD + MIN_BLOCK_SIZE, ALIGNMENT)) {
        /* Shrink the current block */
        write_block(block, size, 1);

        /* Create a new free block from the remainder */
        block_header_t *new_block = NEXT_BLOCK(block);
        write_block(new_block, remainder, 0);
        free_list_insert(new_block);
    } else {
        /* Remainder too small to split — just allocate the entire block */
        write_block(block, block_size, 1);
    }
}

/* ---------------------------------------------------------------
 * Internal: Coalescing
 * --------------------------------------------------------------- */

/*
 * Coalesce a free block with its physically adjacent neighbors.
 * Returns a pointer to the resulting (possibly larger) free block.
 *
 * Cases:
 *   1. Neither neighbor is free          → just return block
 *   2. Next block is free                → merge with next
 *   3. Previous block is free            → merge with prev
 *   4. Both neighbors are free           → merge all three
 */
static block_header_t *coalesce(block_header_t *block) {
    block_header_t *next = NEXT_BLOCK(block);
    int next_alloc = GET_ALLOC(next);

    size_t *prev_footer = PREV_FOOTER(block);
    int prev_alloc = (*prev_footer) & 0x1;

    size_t size = GET_SIZE(block);

    if (prev_alloc && next_alloc) {
        /* Case 1: no coalescing needed */
    } else if (prev_alloc && !next_alloc) {
        /* Case 2: merge with next block */
        free_list_remove(next);
        size += GET_SIZE(next);
        write_block(block, size, 0);
    } else if (!prev_alloc && next_alloc) {
        /* Case 3: merge with previous block */
        block_header_t *prev = PREV_BLOCK(block);
        free_list_remove(prev);
        size += GET_SIZE(prev);
        write_block(prev, size, 0);
        block = prev;
    } else {
        /* Case 4: merge with both neighbors */
        block_header_t *prev = PREV_BLOCK(block);
        free_list_remove(prev);
        free_list_remove(next);
        size += GET_SIZE(prev) + GET_SIZE(next);
        write_block(prev, size, 0);
        block = prev;
    }

    return block;
}

/* ---------------------------------------------------------------
 * Internal: Heap Extension
 * --------------------------------------------------------------- */

/*
 * Extend the heap by `size` bytes using sbrk().
 * Returns a pointer to the new free block, or NULL on failure.
 */
static block_header_t *extend_heap(size_t size) {
    /* Round up to ALIGNMENT */
    size = ALIGN(size, ALIGNMENT);
    if (size < HEAP_GROW_SIZE) {
        size = HEAP_GROW_SIZE;
    }

    void *ptr = sbrk((intptr_t)size);
    if (ptr == (void *)-1) {
        return NULL;
    }

    /* The new block starts where the old epilogue was */
    block_header_t *block = heap_epilogue;
    write_block(block, size, 0);

    /* Write new epilogue at the end of the extended heap */
    heap_epilogue = NEXT_BLOCK(block);
    heap_epilogue->size = PACK(0, 1);

    /* Coalesce with the previous block if it was free */
    block = coalesce(block);
    free_list_insert(block);

    return block;
}

/* ---------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------- */

void my_init(void) {
    if (initialized) {
        return;
    }

    /*
     * Request initial heap space.
     * Layout: [padding] [prologue header+footer] [initial free block] [epilogue header]
     */
    size_t init_size = ALIGN(INITIAL_HEAP_SIZE, ALIGNMENT);

    void *heap_start = sbrk((intptr_t)(init_size));
    if (heap_start == (void *)-1) {
        fprintf(stderr, "my_init: sbrk failed\n");
        return;
    }

    /* Align the start of our heap */
    char *aligned_start = (char *)ALIGN((uintptr_t)heap_start, ALIGNMENT);

    /* --- Prologue block (allocated sentinel, minimum size) --- */
    heap_prologue = (block_header_t *)aligned_start;
    size_t prologue_size = ALIGN(OVERHEAD, ALIGNMENT);
    write_block(heap_prologue, prologue_size, 1);

    /* --- Initial free block --- */
    block_header_t *free_block = NEXT_BLOCK(heap_prologue);
    size_t remaining = init_size - (size_t)(((char *)free_block) - (char *)heap_start);
    /* Reserve space for epilogue */
    size_t free_size = remaining - ALIGN(HEADER_SIZE, ALIGNMENT);
    free_size = ALIGN(free_size, ALIGNMENT);
    write_block(free_block, free_size, 0);

    /* --- Epilogue (allocated sentinel, size = 0) --- */
    heap_epilogue = NEXT_BLOCK(free_block);
    heap_epilogue->size = PACK(0, 1);

    /* Initialize the free list with the single free block */
    free_list_head = NULL;
    free_list_insert(free_block);

    initialized = 1;
}

void *my_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    if (!initialized) {
        my_init();
    }

    size_t adjusted = adjust_size(size);

    /* Search the free list for a suitable block */
    block_header_t *block = find_fit(adjusted);

    if (block != NULL) {
        /* Found a fit — remove from free list, split if possible */
        free_list_remove(block);
        split_block(block, adjusted);
        return PAYLOAD(block);
    }

    /* No fit found — extend the heap */
    block = extend_heap(adjusted);
    if (block == NULL) {
        return NULL; /* sbrk failed */
    }

    free_list_remove(block);
    split_block(block, adjusted);
    return PAYLOAD(block);
}

void my_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    block_header_t *block = HEADER(ptr);

    /* Mark as free */
    size_t size = GET_SIZE(block);
    write_block(block, size, 0);

    /* Coalesce with neighbors and insert into free list */
    block = coalesce(block);
    free_list_insert(block);
}

void *my_calloc(size_t num, size_t size) {
    /* Check for multiplication overflow */
    if (num != 0 && size > (size_t)-1 / num) {
        return NULL;
    }

    size_t total = num * size;
    void *ptr = my_malloc(total);
    if (ptr != NULL) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *my_realloc(void *ptr, size_t size) {
    /* realloc(NULL, size) == malloc(size) */
    if (ptr == NULL) {
        return my_malloc(size);
    }

    /* realloc(ptr, 0) == free(ptr) */
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    block_header_t *block = HEADER(ptr);
    size_t old_size = GET_SIZE(block);
    size_t old_payload = old_size - OVERHEAD;
    size_t new_size = adjust_size(size);

    /* Case 1: Current block is large enough */
    if (old_size >= new_size) {
        /* Optionally split the excess */
        split_block(block, new_size);
        return ptr;
    }

    /* Case 2: Next block is free and combined size is enough */
    block_header_t *next = NEXT_BLOCK(block);
    if (!GET_ALLOC(next)) {
        size_t combined = old_size + GET_SIZE(next);
        if (combined >= new_size) {
            free_list_remove(next);
            write_block(block, combined, 1);
            split_block(block, new_size);
            return ptr;
        }
    }

    /* Case 3: Must relocate — allocate new, copy, free old */
    void *new_ptr = my_malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }

    size_t copy_size = (old_payload < size) ? old_payload : size;
    memcpy(new_ptr, ptr, copy_size);
    my_free(ptr);
    return new_ptr;
}
