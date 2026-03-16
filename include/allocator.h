/*
 * allocator.h — Custom Memory Allocator
 *
 * A drop-in replacement for malloc/free/calloc/realloc using an
 * explicit free list with boundary-tag coalescing.
 *
 * Author: Sheehan
 */

#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>

/* ---------------------------------------------------------------
 * Configuration
 * --------------------------------------------------------------- */

/* Alignment guarantee for all returned pointers (must be power of 2) */
#define ALIGNMENT 16

/* Minimum payload size for a block (prevents over-splitting) */
#define MIN_BLOCK_SIZE 32

/* Initial heap size requested via sbrk (bytes) */
#define INITIAL_HEAP_SIZE 4096

/* Heap growth increment when we run out of space */
#define HEAP_GROW_SIZE 4096

/* Allocation strategy — define USE_BEST_FIT at compile time to switch */
#ifdef USE_BEST_FIT
  #define STRATEGY_BEST_FIT 1
#else
  #define STRATEGY_BEST_FIT 0
#endif

/* ---------------------------------------------------------------
 * Block Header
 *
 * Layout of a block in memory:
 *
 *   +-------------------+
 *   | header (size|alloc)|   <-- block_header_t
 *   +-------------------+
 *   | next (free only)  |
 *   +-------------------+
 *   | prev (free only)  |
 *   +-------------------+
 *   |                   |
 *   |    payload ...     |
 *   |                   |
 *   +-------------------+
 *   | footer (size|alloc)|   <-- boundary tag (size_t)
 *   +-------------------+
 *
 * The low bit of `size` is used as the allocation flag:
 *   bit 0 = 1  →  allocated
 *   bit 0 = 0  →  free
 * --------------------------------------------------------------- */

typedef struct block_header {
    size_t size;                /* block size (including header+footer) | alloc bit */
    struct block_header *next;  /* next block in explicit free list */
    struct block_header *prev;  /* prev block in explicit free list */
} block_header_t;

/* ---------------------------------------------------------------
 * Macros for block manipulation
 * --------------------------------------------------------------- */

/* Round up `x` to the nearest multiple of `align` */
#define ALIGN(x, align) (((x) + (align) - 1) & ~((align) - 1))

/* Header overhead: the size field (we keep next/prev only for free blocks,
   but we always reserve space so the struct size is constant) */
#define HEADER_SIZE (sizeof(block_header_t))
#define FOOTER_SIZE (sizeof(size_t))
#define OVERHEAD   (HEADER_SIZE + FOOTER_SIZE)

/* Extract size and allocation status from a header's size field */
#define GET_SIZE(header)   ((header)->size & ~0x7)
#define GET_ALLOC(header)  ((header)->size & 0x1)

/* Pack a size and allocated bit into a single value */
#define PACK(size, alloc)  ((size) | (alloc))

/* Given a block header, get a pointer to its footer */
#define GET_FOOTER(header) \
    ((size_t *)((char *)(header) + GET_SIZE(header) - FOOTER_SIZE))

/* Given a block header, get the next block in the heap (physically) */
#define NEXT_BLOCK(header) \
    ((block_header_t *)((char *)(header) + GET_SIZE(header)))

/* Given a block header, get the previous block's footer,
   then derive the previous block's header */
#define PREV_FOOTER(header) \
    ((size_t *)((char *)(header) - FOOTER_SIZE))

#define PREV_BLOCK(header) \
    ((block_header_t *)((char *)(header) - (*PREV_FOOTER(header) & ~0x7)))

/* Given a header pointer, get the payload pointer */
#define PAYLOAD(header) ((void *)((char *)(header) + HEADER_SIZE))

/* Given a payload pointer, get the header pointer */
#define HEADER(payload) ((block_header_t *)((char *)(payload) - HEADER_SIZE))

/* ---------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------- */

/* Initialize the allocator — must be called before any allocation */
void  my_init(void);

/* Core allocation functions */
void *my_malloc(size_t size);
void  my_free(void *ptr);
void *my_calloc(size_t num, size_t size);
void *my_realloc(void *ptr, size_t size);

/* Debug / diagnostic utilities */
void  my_heap_dump(void);   /* Print a visual dump of the heap */
int   my_heap_check(void);  /* Verify heap invariants; returns 0 if OK */

#endif /* ALLOCATOR_H */
