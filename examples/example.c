/*
 * example.c — Example Usage of the Custom Memory Allocator
 *
 * Demonstrates basic allocation, deallocation, and heap inspection.
 *
 * Build:  make all && gcc -Wall -Iinclude -o build/example examples/example.c build/*.o
 * Run:    ./build/example
 *
 * Author: Sheehan
 */

#include "allocator.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    my_init();

    printf("=== Custom Memory Allocator Demo ===\n\n");

    /* --- Basic allocation --- */
    printf("1. Allocating three blocks...\n");
    char *name   = (char *)my_malloc(32);
    int  *scores = (int *)my_malloc(5 * sizeof(int));
    char *msg    = (char *)my_malloc(64);

    strcpy(name, "Sheehan");
    for (int i = 0; i < 5; i++) scores[i] = (i + 1) * 10;
    strcpy(msg, "Hello from a custom allocator!");

    printf("   name   = \"%s\"\n", name);
    printf("   scores = [%d, %d, %d, %d, %d]\n",
           scores[0], scores[1], scores[2], scores[3], scores[4]);
    printf("   msg    = \"%s\"\n\n", msg);

    /* --- Show heap state --- */
    printf("2. Heap state after allocations:\n");
    my_heap_dump();

    /* --- Free and observe coalescing --- */
    printf("3. Freeing 'scores' (middle block)...\n");
    my_free(scores);
    my_heap_dump();

    printf("4. Freeing 'name' (should coalesce with 'scores')...\n");
    my_free(name);
    my_heap_dump();

    /* --- Realloc demo --- */
    printf("5. Reallocating 'msg' from 64 to 256 bytes...\n");
    msg = (char *)my_realloc(msg, 256);
    strcat(msg, " Now with more space!");
    printf("   msg = \"%s\"\n\n", msg);

    /* --- Calloc demo --- */
    printf("6. Allocating a zeroed array with calloc...\n");
    int *matrix = (int *)my_calloc(10, sizeof(int));
    int all_zero = 1;
    for (int i = 0; i < 10; i++) {
        if (matrix[i] != 0) all_zero = 0;
    }
    printf("   calloc(10, %zu) -> all zeroed: %s\n\n",
           sizeof(int), all_zero ? "YES" : "NO");

    /* --- Heap check --- */
    printf("7. Running heap consistency check...\n");
    int errors = my_heap_check();
    printf("   Result: %s (%d errors)\n\n", errors ? "FAIL" : "PASS", errors);

    /* --- Cleanup --- */
    my_free(msg);
    my_free(matrix);
    printf("8. Final heap state (all freed):\n");
    my_heap_dump();

    return 0;
}
