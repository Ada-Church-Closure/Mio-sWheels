/*
 * Threaded tests for mpmc_queue (Acutest: https://github.com/mity/acutest).
 * Build from lock-free-queue/: make test
 */

#include "../../tests/vendor/acutest.h"

#include "../mpmc.h"

#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    mpmc_queue_t *q;
    atomic_size_t *next_id;
    size_t total;
} producer_args_t;

typedef struct {
    mpmc_queue_t *q;
    atomic_size_t *received;
    _Atomic uint64_t *sum;
    size_t total;
} consumer_args_t;

static void *producer_main(void *arg) {
    producer_args_t *a = (producer_args_t *)arg;
    for (;;) {
        size_t id = atomic_fetch_add_explicit(a->next_id, 1, memory_order_relaxed);
        if (id >= a->total) {
            break;
        }
        void *payload = (void *)((uintptr_t)(id + 1U));
        while (queue_enqueue(a->q, payload) == 0) {
            sched_yield();
        }
    }
    return NULL;
}

static void *consumer_main(void *arg) {
    consumer_args_t *a = (consumer_args_t *)arg;
    for (;;) {
        if (atomic_load_explicit(a->received, memory_order_relaxed) >= a->total) {
            break;
        }
        void *p = NULL;
        int r = queue_dequeue(a->q, &p);
        if (r == 0) {
            sched_yield();
            continue;
        }
        TEST_ASSERT(r == 1);
        uintptr_t v = (uintptr_t)p;
        TEST_ASSERT(v >= 1U);
        atomic_fetch_add_explicit(a->sum, (uint64_t)v, memory_order_relaxed);
        atomic_fetch_add_explicit(a->received, 1, memory_order_relaxed);
    }
    return NULL;
}

static void test_single_thread_fifo(void) {
    mpmc_queue_t q;
    TEST_ASSERT(queue_init(&q, 16) == 0);

    int x = 42;
    int y = 43;
    TEST_ASSERT(queue_enqueue(&q, &x) == 1);
    TEST_ASSERT(queue_enqueue(&q, &y) == 1);

    void *out = NULL;
    TEST_ASSERT(queue_dequeue(&q, &out) == 1);
    TEST_ASSERT(out == &x);
    TEST_ASSERT(queue_dequeue(&q, &out) == 1);
    TEST_ASSERT(out == &y);

    void *z = NULL;
    TEST_ASSERT(queue_dequeue(&q, &z) == 0);

    TEST_ASSERT(queue_destroy(&q) == 0);
}

static void test_mpmc_sum_stress(void) {
    enum { CAP = 1024, TOTAL = 200000, NPROD = 4, NCONS = 4 };

    mpmc_queue_t q;
    TEST_ASSERT(queue_init(&q, CAP) == 0);

    atomic_size_t next_id;
    atomic_init(&next_id, 0);
    atomic_size_t received;
    atomic_init(&received, 0);
    _Atomic uint64_t sum;
    atomic_init(&sum, 0ULL);

    producer_args_t pa = {.q = &q, .next_id = &next_id, .total = TOTAL};
    consumer_args_t ca = {.q = &q, .received = &received, .sum = &sum, .total = TOTAL};

    pthread_t producers[NPROD];
    pthread_t consumers[NCONS];

    for (int i = 0; i < NCONS; i++) {
        TEST_ASSERT(pthread_create(&consumers[i], NULL, consumer_main, &ca) == 0);
    }
    for (int i = 0; i < NPROD; i++) {
        TEST_ASSERT(pthread_create(&producers[i], NULL, producer_main, &pa) == 0);
    }

    for (int i = 0; i < NPROD; i++) {
        TEST_ASSERT(pthread_join(producers[i], NULL) == 0);
    }
    for (int i = 0; i < NCONS; i++) {
        TEST_ASSERT(pthread_join(consumers[i], NULL) == 0);
    }

    uint64_t expected = (uint64_t)TOTAL * (uint64_t)(TOTAL + 1U) / 2U;
    TEST_ASSERT(atomic_load_explicit(&received, memory_order_relaxed) == (size_t)TOTAL);
    TEST_ASSERT(atomic_load_explicit(&sum, memory_order_relaxed) == expected);

    TEST_ASSERT(queue_destroy(&q) == 0);
}

TEST_LIST = {
    {"single_thread_fifo", test_single_thread_fifo},
    {"mpmc_sum_stress", test_mpmc_sum_stress},
    {NULL, NULL},
};
