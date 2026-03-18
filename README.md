# Custom Memory Allocator in C

A from-scratch implementation of `malloc`, `free`, `calloc`, and `realloc` in C, designed to deepen understanding of low-level systems programming and memory management.

## Features

- **Explicit free list** with LIFO insertion for O(1) free operations
- **Boundary-tag coalescing** for O(1) merge with adjacent free blocks
- **Block splitting** to minimize internal fragmentation
- **Configurable allocation strategy** — first-fit (default) or best-fit
- **16-byte alignment** guarantee on all returned pointers
- **Heap growth** via `sbrk()` with automatic extension
- **Debug utilities** — heap dump visualization and consistency checker
- **Benchmarking harness** — throughput and fragmentation metrics vs. system `malloc`

## Project Structure

```
├── include/
│   └── allocator.h        # Public API, block metadata, macros
├── src/
│   ├── allocator.c        # Core allocator implementation
│   └── debug.c            # Heap dump & consistency checker
├── tests/
│   ├── test_basic.c       # Unit tests for malloc/free/calloc/realloc
│   ├── test_coalesce.c    # Coalescing & splitting edge cases
│   └── test_stress.c      # Random alloc/free stress tests
├── benchmarks/
│   └── bench.c            # Throughput & fragmentation benchmarks
└── Makefile
```

## Building

```bash
# Build the allocator
make all

# Build with best-fit strategy instead of first-fit
make all STRATEGY=best-fit

# Run tests
make test

# Run benchmarks
make bench

# Clean build artifacts
make clean
```

## Architecture

### Block Layout

Every block (allocated or free) carries metadata:

```
+--------------------+
| Header             |  ← size (with alloc bit in LSB)
| next (free only)   |  ← pointer to next free block
| prev (free only)   |  ← pointer to prev free block
+--------------------+
|                    |
|   Payload / Data   |  ← 16-byte aligned
|                    |
+--------------------+
| Footer             |  ← copy of size | alloc (boundary tag)
+--------------------+
```

### Allocation Strategy

| Strategy    | Description | Tradeoff |
|------------|-------------|----------|
| **First-fit** | Returns the first block large enough | Faster search, more fragmentation |
| **Best-fit**  | Returns the smallest block large enough | Slower search, less fragmentation |

### Coalescing

When a block is freed, the allocator immediately checks both neighbors using boundary tags:

- **Next block free** → merge forward
- **Previous block free** → merge backward
- **Both free** → merge all three into one block
- **Neither free** → insert as-is into the free list

## Requirements

- GCC (or compatible C11 compiler)
- POSIX environment (Linux, macOS, or WSL on Windows) — uses `sbrk()`