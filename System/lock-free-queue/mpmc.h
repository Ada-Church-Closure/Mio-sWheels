#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdint.h>

#define CACHE_LINE_SIZE 64

typedef struct {
    atomic_size_t sequence_number;
    void* data;
} cell_t;

typedef struct {
    size_t buffer_mask; // mask_number
    cell_t* buffer; // ring buffer

    _Alignas(CACHE_LINE_SIZE) atomic_size_t enqueue_pos; // cpu cache line friendly.
    _Alignas(CACHE_LINE_SIZE) atomic_size_t dequeue_pos;
} mpmc_queue_t;

// init queue
int queue_init(mpmc_queue_t* q, size_t capacity);

// enqueue
int queue_enqueue(mpmc_queue_t* q, void* data);

// dequeue
int queue_dequeue(mpmc_queue_t* q, void** data);

// free queue
int queue_destroy(mpmc_queue_t* q);



