/*
 * High-load throughput benchmark for mpmc_queue (not a correctness suite).
 * Usage: see print_usage(). Build: make bench
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../mpmc.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    mpmc_queue_t *q;
    atomic_size_t next_id;
    atomic_size_t received;
    _Atomic uint64_t sum;
    size_t total;
    int verify;
    pthread_barrier_t *barrier;
    struct timespec *t0;
    struct timespec *t1;
} bench_shared_t;

typedef struct {
    bench_shared_t *s;
} bench_thread_arg_t;

static double timespec_diff_sec(const struct timespec *a, const struct timespec *b) {
    return (double)(b->tv_sec - a->tv_sec) + (double)(b->tv_nsec - a->tv_nsec) / 1e9;
}

static void *producer_loop(void *arg) {
    bench_thread_arg_t *t = (bench_thread_arg_t *)arg;
    bench_shared_t *s = t->s;

    int br = pthread_barrier_wait(s->barrier);
    if (br != 0 && br != PTHREAD_BARRIER_SERIAL_THREAD) {
        fprintf(stderr, "pthread_barrier_wait: %d\n", br);
        abort();
    }
    if (br == PTHREAD_BARRIER_SERIAL_THREAD) {
        if (clock_gettime(CLOCK_MONOTONIC, s->t0) != 0) {
            perror("clock_gettime");
            abort();
        }
    }

    for (;;) {
        size_t id = atomic_fetch_add_explicit(&s->next_id, 1, memory_order_relaxed);
        if (id >= s->total) {
            break;
        }
        void *payload = (void *)((uintptr_t)(id + 1U));
        while (queue_enqueue(s->q, payload) == 0) {
            /* spin on full */
        }
    }
    return NULL;
}

static void *consumer_loop(void *arg) {
    bench_thread_arg_t *t = (bench_thread_arg_t *)arg;
    bench_shared_t *s = t->s;

    int br = pthread_barrier_wait(s->barrier);
    if (br != 0 && br != PTHREAD_BARRIER_SERIAL_THREAD) {
        fprintf(stderr, "pthread_barrier_wait: %d\n", br);
        abort();
    }
    if (br == PTHREAD_BARRIER_SERIAL_THREAD) {
        if (clock_gettime(CLOCK_MONOTONIC, s->t0) != 0) {
            perror("clock_gettime");
            abort();
        }
    }

    for (;;) {
        size_t cur = atomic_load_explicit(&s->received, memory_order_relaxed);
        if (cur >= s->total) {
            break;
        }
        void *p = NULL;
        int r = queue_dequeue(s->q, &p);
        if (r == 0) {
            continue;
        }
        if (r != 1) {
            fprintf(stderr, "unexpected dequeue return %d\n", r);
            abort();
        }
        uintptr_t v = (uintptr_t)p;
        if (s->verify) {
            atomic_fetch_add_explicit(&s->sum, (uint64_t)v, memory_order_relaxed);
        }
        size_t k = atomic_fetch_add_explicit(&s->received, 1, memory_order_relaxed) + 1U;
        if (k == s->total) {
            if (clock_gettime(CLOCK_MONOTONIC, s->t1) != 0) {
                perror("clock_gettime");
                abort();
            }
        }
    }
    return NULL;
}

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [--verify] [total_ops capacity nprod ncons]\n"
            "  High-throughput MPMC benchmark. Defaults tuned for a strong load.\n"
            "  --verify   check sum == n(n+1)/2 after run (adds atomic work on consumers)\n"
            "  total_ops  default %zu\n"
            "  capacity   ring size (power of two), default %zu\n"
            "  nprod      producer threads, default %u\n"
            "  ncons      consumer threads, default %u\n",
            prog, (size_t)50000000, (size_t)8192, 16U, 16U);
}

int main(int argc, char **argv) {
    int verify = 0;
    int ai = 1;
    if (argc > 1 && strcmp(argv[1], "--verify") == 0) {
        verify = 1;
        ai++;
    }

    size_t total = 50000000;
    size_t capacity = 8192;
    unsigned nprod = 16;
    unsigned ncons = 16;

    if (argc > ai && strcmp(argv[ai], "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    if (argc > ai) {
        total = (size_t)strtoull(argv[ai], NULL, 10);
        ai++;
    }
    if (argc > ai) {
        capacity = (size_t)strtoull(argv[ai], NULL, 10);
        ai++;
    }
    if (argc > ai) {
        nprod = (unsigned)strtoul(argv[ai], NULL, 10);
        ai++;
    }
    if (argc > ai) {
        ncons = (unsigned)strtoul(argv[ai], NULL, 10);
        ai++;
    }
    if (argc > ai) {
        fprintf(stderr, "Too many arguments.\n");
        print_usage(argv[0]);
        return 2;
    }
    if (total == 0 || capacity < 2 || nprod == 0 || ncons == 0) {
        print_usage(argv[0]);
        return 2;
    }
    if ((capacity & (capacity - 1U)) != 0) {
        fprintf(stderr, "capacity must be a power of two\n");
        return 2;
    }

    mpmc_queue_t q;
    if (queue_init(&q, capacity) != 0) {
        fprintf(stderr, "queue_init failed\n");
        return 1;
    }

    struct timespec t0;
    struct timespec t1;
    memset(&t0, 0, sizeof(t0));
    memset(&t1, 0, sizeof(t1));

    bench_shared_t shared;
    shared.q = &q;
    atomic_init(&shared.next_id, 0);
    atomic_init(&shared.received, 0);
    atomic_init(&shared.sum, 0ULL);
    shared.total = total;
    shared.verify = verify;
    shared.t0 = &t0;
    shared.t1 = &t1;

    unsigned nthreads = nprod + ncons;
    pthread_barrier_t barrier;
    if (pthread_barrier_init(&barrier, NULL, (unsigned)nthreads) != 0) {
        perror("pthread_barrier_init");
        queue_destroy(&q);
        return 1;
    }
    shared.barrier = &barrier;

    pthread_t *threads = (pthread_t *)calloc(nthreads, sizeof(pthread_t));
    bench_thread_arg_t *args = (bench_thread_arg_t *)calloc(nthreads, sizeof(bench_thread_arg_t));
    if (!threads || !args) {
        fprintf(stderr, "out of memory\n");
        free(threads);
        free(args);
        pthread_barrier_destroy(&barrier);
        queue_destroy(&q);
        return 1;
    }

    unsigned created = 0;
    for (unsigned i = 0; i < nprod; i++) {
        args[i].s = &shared;
        if (pthread_create(&threads[i], NULL, producer_loop, &args[i]) != 0) {
            perror("pthread_create");
            goto fail;
        }
        created++;
    }
    for (unsigned i = 0; i < ncons; i++) {
        unsigned j = nprod + i;
        args[j].s = &shared;
        if (pthread_create(&threads[j], NULL, consumer_loop, &args[j]) != 0) {
            perror("pthread_create");
            goto fail;
        }
        created++;
    }

    for (unsigned i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }

    if (t0.tv_sec == 0 && t0.tv_nsec == 0) {
        fprintf(stderr, "internal error: t0 not set\n");
        goto fail_after_threads;
    }
    if (t1.tv_sec == 0 && t1.tv_nsec == 0) {
        fprintf(stderr, "internal error: t1 not set\n");
        goto fail_after_threads;
    }

    double sec = timespec_diff_sec(&t0, &t1);
    if (sec <= 0.0) {
        sec = 1e-9;
    }
    double mops = (double)total / sec / 1e6;

    printf("mpmc bench: total_ops=%zu capacity=%zu nprod=%u ncons=%u verify=%s\n", total, capacity,
           nprod, ncons, verify ? "on" : "off");
    printf("elapsed: %.6f s  throughput: %.3f Mops/s  (%.2f ns/op)\n", sec, mops, 1e9 * sec / (double)total);

    if (verify) {
        uint64_t expected = (uint64_t)total * (uint64_t)(total + 1U) / 2U;
        uint64_t got = atomic_load_explicit(&shared.sum, memory_order_relaxed);
        if (got != expected) {
            fprintf(stderr, "VERIFY FAIL: sum=%" PRIu64 " expected=%" PRIu64 "\n", got, expected);
            free(threads);
            free(args);
            pthread_barrier_destroy(&barrier);
            queue_destroy(&q);
            return 3;
        }
        printf("verify: sum OK\n");
    }

    free(threads);
    free(args);
    pthread_barrier_destroy(&barrier);
    queue_destroy(&q);
    return 0;

fail:
    for (unsigned i = 0; i < created; i++) {
        pthread_join(threads[i], NULL);
    }
fail_after_threads:
    free(threads);
    free(args);
    pthread_barrier_destroy(&barrier);
    queue_destroy(&q);
    return 1;
}
