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
 * Find a suitable free block using the configured strategy.
 * (Best-fit will be added in a later commit.)
 */
static block_header_t *find_fit(size_t size) {
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

    /* Insert the new block into the free list */
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

/*
 * Placeholder — will be implemented in the next commit
 * along with boundary-tag coalescing.
 */
void my_free(void *ptr) {
    (void)ptr;
    fprintf(stderr, "my_free: not yet implemented\n");
}

void *my_calloc(size_t num, size_t size) {
    (void)num;
    (void)size;
    fprintf(stderr, "my_calloc: not yet implemented\n");
    return NULL;
}

void *my_realloc(void *ptr, size_t size) {
    (void)ptr;
    (void)size;
    fprintf(stderr, "my_realloc: not yet implemented\n");
    return NULL;
}
