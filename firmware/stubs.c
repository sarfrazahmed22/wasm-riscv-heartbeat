#include <stdint.h>

void _exit(int status) { while(1); }

/* Heap for picolibc (not used by Wasm3 which has its own fixed heap) */
static char heap[4096] __attribute__((aligned(8)));
char *__heap_start = heap;
char *__heap_end   = heap + sizeof(heap);
