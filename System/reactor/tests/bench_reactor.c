/*
 * Throughput / stress benchmarks for reactor (socketpair + read/write path).
 *
 * Quick (default):
 *   ./tests/bench_reactor [iterations] [payload_bytes]
 *
 * Stress modes:
 *   ./tests/bench_reactor duration <wall_seconds> [payload_bytes]
 *   ./tests/bench_reactor flood <iterations> [payload_bytes]
 *   ./tests/bench_reactor mux <iterations> [payload_bytes] [connections]
 *
 * Build: make bench | make stress
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../reactor.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define EPOLL_WAIT_MS 50
#define PAYLOAD_CAP 1024u
#define MUX_MAX_CONN 512

static double timespec_diff_sec(const struct timespec *a, const struct timespec *b) {
    return (double)(b->tv_sec - a->tv_sec) + (double)(b->tv_nsec - a->tv_nsec) / 1e9;
}

static int echo_once(aeEventLoop *loop, int peer, const unsigned char *data, size_t len, int max_steps) {
    if (write(peer, data, len) != (ssize_t)len) return -1;

    unsigned char rx[65536];
    size_t got = 0;

    for (int step = 0; step < max_steps; ++step) {
        if (aeProcessEvents(loop, EPOLL_WAIT_MS) < 0) return -2;

        for (;;) {
            ssize_t n = read(peer, rx + got, sizeof(rx) - got);
            if (n > 0) {
                got += (size_t)n;
                if (got == len) return memcmp(rx, data, len) == 0 ? 0 : -3;
                continue;
            }
            if (n == 0) return -4;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return -5;
        }
    }
    return -6;
}

static int fill_payload(unsigned char *payload, size_t payload_len) {
    if (payload_len == 0 || payload_len > PAYLOAD_CAP) return -1;
    for (size_t i = 0; i < payload_len; ++i) payload[i] = (unsigned char)((i * 13) & 0xff);
    return 0;
}

static void print_usage(const char *argv0) {
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  %s [iterations] [payload_bytes]\n", argv0);
    fprintf(stderr, "  %s duration <wall_seconds> [payload_bytes]\n", argv0);
    fprintf(stderr, "  %s flood <iterations> [payload_bytes]\n", argv0);
    fprintf(stderr, "  %s mux <iterations> [payload_bytes] [connections]\n", argv0);
    fprintf(stderr, "defaults: iterations=50000 payload=64; payload must be 1..%u (reactor read buf 1024)\n",
            (unsigned)PAYLOAD_CAP);
}

typedef struct {
    int sp[2];
    aeClient *client;
} mux_chan_t;

static void mux_teardown(aeEventLoop *loop, mux_chan_t *ch, int n) {
    if (!ch) return;
    for (int i = 0; i < n; ++i) {
        if (ch[i].sp[0] >= 0) aeDeleteFileEvent(loop, ch[i].sp[0], AE_READABLE | AE_WRITABLE);
        if (ch[i].sp[0] >= 0) close(ch[i].sp[0]);
        if (ch[i].sp[1] >= 0) close(ch[i].sp[1]);
        aeClientDestroy(ch[i].client);
    }
    free(ch);
}

static int run_mux(uint64_t iterations, size_t payload_len, int nconn) {
    if (nconn < 1) nconn = 1;
    if (nconn > MUX_MAX_CONN) nconn = MUX_MAX_CONN;

    unsigned char payload[PAYLOAD_CAP];
    if (fill_payload(payload, payload_len) != 0) return 1;

    aeEventLoop *loop = aeCreateEventLoop();
    if (!loop) {
        fprintf(stderr, "aeCreateEventLoop failed\n");
        return 1;
    }

    mux_chan_t *ch = (mux_chan_t *)calloc((size_t)nconn, sizeof(mux_chan_t));
    if (!ch) {
        fprintf(stderr, "calloc mux channels\n");
        aeDestroyEventLoop(loop);
        return 1;
    }
    for (int i = 0; i < nconn; ++i) {
        ch[i].sp[0] = -1;
        ch[i].sp[1] = -1;
        ch[i].client = NULL;
    }

    for (int i = 0; i < nconn; ++i) {
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, ch[i].sp) != 0) {
            perror("socketpair");
            mux_teardown(loop, ch, i);
            aeDestroyEventLoop(loop);
            return 1;
        }
        setNonBlock(ch[i].sp[0]);
        setNonBlock(ch[i].sp[1]);
        ch[i].client = aeClientCreate();
        if (!ch[i].client || aeCreateFileEvent(loop, ch[i].sp[0], AE_READABLE, readQueryFromClient, ch[i].client) != 0) {
            fprintf(stderr, "mux setup failed at i=%d\n", i);
            mux_teardown(loop, ch, i + 1);
            aeDestroyEventLoop(loop);
            return 1;
        }
    }

    struct timespec t0, t1;
    if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0) {
        perror("clock_gettime");
        mux_teardown(loop, ch, nconn);
        aeDestroyEventLoop(loop);
        return 1;
    }

    int max_steps = payload_len <= 512 ? 48 : 256;
    for (uint64_t j = 0; j < iterations; ++j) {
        int i = (int)(j % (uint64_t)nconn);
        int er = echo_once(loop, ch[i].sp[1], payload, payload_len, max_steps);
        if (er != 0) {
            fprintf(stderr, "mux echo_once failed j=%" PRIu64 " ch=%d code=%d\n", j, i, er);
            mux_teardown(loop, ch, nconn);
            aeDestroyEventLoop(loop);
            return 1;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
        perror("clock_gettime");
        mux_teardown(loop, ch, nconn);
        aeDestroyEventLoop(loop);
        return 1;
    }

    double sec = timespec_diff_sec(&t0, &t1);
    if (sec <= 0) sec = 1e-9;
    double rps = (double)iterations / sec;
    double mb_s = ((double)iterations * (double)payload_len * 2.0) / (sec * 1024.0 * 1024.0);

    printf("mode=mux connections=%d iterations=%" PRIu64 " payload=%zu wall=%.6f s\n", nconn, iterations, payload_len, sec);
    printf("round_trips_per_sec=%.0f  approx_goodput_MiB_s=%.2f\n", rps, mb_s);

    mux_teardown(loop, ch, nconn);
    aeDestroyEventLoop(loop);
    return 0;
}

static int run_quick_or_flood(uint64_t iterations, size_t payload_len, int progress, const char *label) {
    int rc = 0;
    unsigned char *payload = (unsigned char *)malloc(payload_len);
    if (!payload) {
        fprintf(stderr, "malloc payload\n");
        return 1;
    }
    if (fill_payload(payload, payload_len) != 0) {
        free(payload);
        return 1;
    }

    int sp[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sp) != 0) {
        perror("socketpair");
        free(payload);
        return 1;
    }
    setNonBlock(sp[0]);
    setNonBlock(sp[1]);

    aeEventLoop *loop = aeCreateEventLoop();
    aeClient *client = aeClientCreate();
    if (!loop || !client) {
        fprintf(stderr, "alloc loop/client failed\n");
        close(sp[0]);
        close(sp[1]);
        free(payload);
        return 1;
    }
    if (aeCreateFileEvent(loop, sp[0], AE_READABLE, readQueryFromClient, client) != 0) {
        fprintf(stderr, "aeCreateFileEvent failed\n");
        aeClientDestroy(client);
        aeDestroyEventLoop(loop);
        close(sp[0]);
        close(sp[1]);
        free(payload);
        return 1;
    }

    struct timespec t0, t1, t_last_print;
    if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0) {
        perror("clock_gettime");
        rc = 1;
        goto cleanup;
    }
    t_last_print = t0;

    int max_steps = payload_len <= 512 ? 48 : 256;
    uint64_t print_step = 0;
    if (progress) {
        print_step = iterations / 20u;
        if (print_step < 1u) print_step = 1u;
        if (iterations >= 1000000u && print_step < 50000u) print_step = 50000u;
    }

    for (uint64_t i = 0; i < iterations; ++i) {
        int er = echo_once(loop, sp[1], payload, payload_len, max_steps);
        if (er != 0) {
            fprintf(stderr, "%s echo_once failed at i=%" PRIu64 " code=%d\n", label, i, er);
            rc = 1;
            goto cleanup;
        }
        if (progress && print_step > 0 && (i + 1u) % print_step == 0) {
            struct timespec now;
            if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) break;
            double dt = timespec_diff_sec(&t0, &now);
            if (dt <= 0) dt = 1e-9;
            fprintf(stderr, "[%s] progress i=%" PRIu64 " (%.1f%%)  interim_rps=%.0f\n", label, i + 1u,
                    100.0 * (double)(i + 1u) / (double)iterations, (double)(i + 1u) / dt);
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
        perror("clock_gettime");
        rc = 1;
        goto cleanup;
    }

    (void)t_last_print;
    double sec = timespec_diff_sec(&t0, &t1);
    if (sec <= 0) sec = 1e-9;
    double rps = (double)iterations / sec;
    double mb_s = ((double)iterations * (double)payload_len * 2.0) / (sec * 1024.0 * 1024.0);

    printf("%s iterations=%" PRIu64 " payload=%zu wall=%.6f s\n", label, iterations, payload_len, sec);
    printf("round_trips_per_sec=%.0f  approx_goodput_MiB_s=%.2f (echo: peer->reactor->peer)\n", rps, mb_s);

cleanup:
    aeDeleteFileEvent(loop, sp[0], AE_READABLE | AE_WRITABLE);
    close(sp[0]);
    close(sp[1]);
    free(payload);
    aeClientDestroy(client);
    aeDestroyEventLoop(loop);
    return rc;
}

static int run_duration(double wall_sec, size_t payload_len) {
    int rc = 0;
    unsigned char *payload = (unsigned char *)malloc(payload_len);
    if (!payload) return 1;
    if (fill_payload(payload, payload_len) != 0) {
        free(payload);
        return 1;
    }

    int sp[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sp) != 0) {
        perror("socketpair");
        free(payload);
        return 1;
    }
    setNonBlock(sp[0]);
    setNonBlock(sp[1]);

    aeEventLoop *loop = aeCreateEventLoop();
    aeClient *client = aeClientCreate();
    if (!loop || !client) {
        aeClientDestroy(client);
        aeDestroyEventLoop(loop);
        close(sp[0]);
        close(sp[1]);
        free(payload);
        return 1;
    }
    if (aeCreateFileEvent(loop, sp[0], AE_READABLE, readQueryFromClient, client) != 0) {
        aeClientDestroy(client);
        aeDestroyEventLoop(loop);
        close(sp[0]);
        close(sp[1]);
        free(payload);
        return 1;
    }

    struct timespec t0, t1, t_last, now;
    if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0) {
        perror("clock_gettime");
        rc = 1;
        goto cleanup_d;
    }
    t_last = t0;

    uint64_t count = 0;
    int max_steps = payload_len <= 512 ? 48 : 256;

    for (;;) {
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
            rc = 1;
            break;
        }
        if (timespec_diff_sec(&t0, &now) >= wall_sec) break;

        int er = echo_once(loop, sp[1], payload, payload_len, max_steps);
        if (er != 0) {
            fprintf(stderr, "duration echo_once failed count=%" PRIu64 " code=%d\n", count, er);
            rc = 1;
            goto cleanup_d;
        }
        count++;

        if (timespec_diff_sec(&t_last, &now) >= 2.0) {
            double span = timespec_diff_sec(&t0, &now);
            if (span <= 0) span = 1e-9;
            fprintf(stderr, "[duration] wall=%.1f/%.1f s  count=%" PRIu64 "  interim_rps=%.0f\n", timespec_diff_sec(&t0, &now),
                    wall_sec, count, (double)count / span);
            t_last = now;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
        perror("clock_gettime");
        rc = 1;
        goto cleanup_d;
    }

    double sec = timespec_diff_sec(&t0, &t1);
    if (sec <= 0) sec = 1e-9;
    double rps = (double)count / sec;
    double mb_s = ((double)count * (double)payload_len * 2.0) / (sec * 1024.0 * 1024.0);

    printf("mode=duration wall_target=%.3f s wall_actual=%.6f s count=%" PRIu64 " payload=%zu\n", wall_sec, sec, count,
           payload_len);
    printf("round_trips_per_sec=%.0f  approx_goodput_MiB_s=%.2f\n", rps, mb_s);

cleanup_d:
    aeDeleteFileEvent(loop, sp[0], AE_READABLE | AE_WRITABLE);
    close(sp[0]);
    close(sp[1]);
    free(payload);
    aeClientDestroy(client);
    aeDestroyEventLoop(loop);
    return rc;
}

static int arg_payload(int argc, char **argv, int idx, size_t *out, size_t def) {
    if (argc <= idx) {
        *out = def;
        return 0;
    }
    unsigned long v = strtoul(argv[idx], NULL, 10);
    if (v == 0 || v > PAYLOAD_CAP) {
        fprintf(stderr, "payload_bytes must be 1..%u\n", (unsigned)PAYLOAD_CAP);
        return -1;
    }
    *out = (size_t)v;
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "duration") == 0) {
        if (argc < 3) {
            print_usage(argv[0]);
            return 1;
        }
        double wall = strtod(argv[2], NULL);
        if (wall <= 0 || wall > 86400.0) {
            fprintf(stderr, "duration: wall_seconds must be in (0, 86400]\n");
            return 1;
        }
        size_t payload_len = 1024;
        if (arg_payload(argc, argv, 3, &payload_len, 1024) != 0) return 1;
        if (argc > 4) {
            print_usage(argv[0]);
            return 1;
        }
        return run_duration(wall, payload_len);
    }

    if (argc >= 2 && strcmp(argv[1], "flood") == 0) {
        if (argc < 3) {
            print_usage(argv[0]);
            return 1;
        }
        uint64_t iterations = strtoull(argv[2], NULL, 10);
        if (iterations == 0) {
            fprintf(stderr, "flood: iterations must be > 0\n");
            return 1;
        }
        size_t payload_len = 1024;
        if (arg_payload(argc, argv, 3, &payload_len, 1024) != 0) return 1;
        if (argc > 4) {
            print_usage(argv[0]);
            return 1;
        }
        return run_quick_or_flood(iterations, payload_len, 1, "mode=flood");
    }

    if (argc >= 2 && strcmp(argv[1], "mux") == 0) {
        if (argc < 3) {
            print_usage(argv[0]);
            return 1;
        }
        uint64_t iterations = strtoull(argv[2], NULL, 10);
        if (iterations == 0) {
            fprintf(stderr, "mux: iterations must be > 0\n");
            return 1;
        }
        size_t payload_len = 1024;
        if (argc >= 4) {
            if (arg_payload(argc, argv, 3, &payload_len, 1024) != 0) return 1;
        } else {
            payload_len = 1024;
        }
        int nconn = 128;
        if (argc >= 5) {
            nconn = (int)strtoul(argv[4], NULL, 10);
            if (nconn <= 0 || nconn > MUX_MAX_CONN) {
                fprintf(stderr, "mux: connections must be 1..%d\n", MUX_MAX_CONN);
                return 1;
            }
        }
        if (argc > 5) {
            print_usage(argv[0]);
            return 1;
        }
        return run_mux(iterations, payload_len, nconn);
    }

    /* Quick mode: numeric argv[1] */
    uint64_t iterations = 50000;
    size_t payload_len = 64;

    if (argc >= 2) {
        char *end = NULL;
        unsigned long long v = strtoull(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0' || v == 0) {
            print_usage(argv[0]);
            return 1;
        }
        iterations = (uint64_t)v;
    }
    if (argc >= 3) {
        if (arg_payload(argc, argv, 2, &payload_len, 64) != 0) return 1;
    }
    if (argc > 3) {
        print_usage(argv[0]);
        return 1;
    }

    return run_quick_or_flood(iterations, payload_len, 0, "mode=quick");
}
