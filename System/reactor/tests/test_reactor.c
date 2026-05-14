/*
 * Boundary / integration tests for reactor (Acutest: ../tests/vendor).
 * Build: make test
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../../tests/vendor/acutest.h"

#include "../reactor.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void pipe_read_accum(struct aeEventLoop *loop, int fd, void *clientData, int mask) {
    (void)loop;
    (void)clientData;
    (void)mask;
    size_t *total = (size_t *)clientData;
    char buf[512];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof buf);
        if (n > 0) {
            *total += (size_t)n;
            continue;
        }
        if (n == 0) return;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        return;
    }
}

static void test_create_destroy_and_poll(void) {
    aeEventLoop *loop = aeCreateEventLoop();
    TEST_ASSERT(loop != NULL);
    TEST_ASSERT(aeProcessEvents(loop, 0) == 0);
    aeDestroyEventLoop(loop);
}

static void test_fd_bounds(void) {
    aeEventLoop *loop = aeCreateEventLoop();
    TEST_ASSERT(loop != NULL);
    TEST_ASSERT(aeCreateFileEvent(loop, -1, AE_READABLE, pipe_read_accum, NULL) == -1);
    TEST_ASSERT(aeCreateFileEvent(loop, MAX_FDS, AE_READABLE, pipe_read_accum, NULL) == -1);
    aeDestroyEventLoop(loop);
}

static void test_delete_on_unregistered_fd(void) {
    int p[2];
    TEST_ASSERT(pipe(p) == 0);
    aeEventLoop *loop = aeCreateEventLoop();
    TEST_ASSERT(aeDeleteFileEvent(loop, p[0], AE_READABLE) == 0);
    close(p[0]);
    close(p[1]);
    aeDestroyEventLoop(loop);
}

static void test_set_nonblock_pipe(void) {
    int p[2];
    TEST_ASSERT(pipe(p) == 0);
    setNonBlock(p[0]);
    int fl = fcntl(p[0], F_GETFL, 0);
    TEST_ASSERT(fl != -1);
    TEST_ASSERT((fl & O_NONBLOCK) != 0);
    close(p[0]);
    close(p[1]);
}

static void test_pipe_readable_dispatch(void) {
    int p[2];
    TEST_ASSERT(pipe(p) == 0);
    setNonBlock(p[0]);
    setNonBlock(p[1]);

    const char *msg = "hello-reactor";
    size_t len = strlen(msg);
    TEST_ASSERT((ssize_t)len == write(p[1], msg, len));

    aeEventLoop *loop = aeCreateEventLoop();
    size_t total = 0;
    TEST_ASSERT(aeCreateFileEvent(loop, p[0], AE_READABLE, pipe_read_accum, &total) == 0);
    TEST_ASSERT(aeProcessEvents(loop, 500) >= 0);
    TEST_ASSERT(total == len);

    aeDeleteFileEvent(loop, p[0], AE_READABLE | AE_WRITABLE);
    close(p[0]);
    close(p[1]);
    aeDestroyEventLoop(loop);
}

/* Echo path: readQueryFromClient + sendReplyToClient over a connected socketpair. */
static int echo_roundtrip(aeEventLoop *loop, int peer, const unsigned char *data, size_t len, int max_steps) {
    ssize_t w = write(peer, data, len);
    if (w != (ssize_t)len) return -1;

    unsigned char rx[32768];
    size_t got = 0;

    for (int step = 0; step < max_steps; ++step) {
        int r = aeProcessEvents(loop, 500);
        if (r < 0) return -2;

        for (;;) {
            ssize_t n = read(peer, rx + got, sizeof(rx) - got);
            if (n > 0) {
                got += (size_t)n;
                if (got == len) {
                    return memcmp(rx, data, len) == 0 ? 0 : -3;
                }
                continue;
            }
            if (n == 0) return -4;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return -5;
        }
    }
    return -6;
}

static void test_socketpair_echo_small(void) {
    int sp[2];
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sp) == 0);
    setNonBlock(sp[0]);
    setNonBlock(sp[1]);

    aeEventLoop *loop = aeCreateEventLoop();
    aeClient *client = aeClientCreate();
    TEST_ASSERT(client != NULL);
    TEST_ASSERT(aeCreateFileEvent(loop, sp[0], AE_READABLE, readQueryFromClient, client) == 0);

    static const unsigned char payload[] = "PING";
    int er = echo_roundtrip(loop, sp[1], payload, sizeof payload - 1u, 64);
    TEST_ASSERT(er == 0);

    aeDeleteFileEvent(loop, sp[0], AE_READABLE | AE_WRITABLE);
    close(sp[0]);
    close(sp[1]);
    aeClientDestroy(client);
    aeDestroyEventLoop(loop);
}

static void test_socketpair_echo_1024_boundary(void) {
    int sp[2];
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sp) == 0);
    setNonBlock(sp[0]);
    setNonBlock(sp[1]);

    aeEventLoop *loop = aeCreateEventLoop();
    aeClient *client = aeClientCreate();
    TEST_ASSERT(client != NULL);
    TEST_ASSERT(aeCreateFileEvent(loop, sp[0], AE_READABLE, readQueryFromClient, client) == 0);

    unsigned char payload[1024];
    for (size_t i = 0; i < sizeof payload; ++i) payload[i] = (unsigned char)(i & 0xff);

    int er = echo_roundtrip(loop, sp[1], payload, sizeof payload, 256);
    TEST_ASSERT(er == 0);

    aeDeleteFileEvent(loop, sp[0], AE_READABLE | AE_WRITABLE);
    close(sp[0]);
    close(sp[1]);
    aeClientDestroy(client);
    aeDestroyEventLoop(loop);
}

static void test_socketpair_echo_1025_two_reads(void) {
    int sp[2];
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sp) == 0);
    setNonBlock(sp[0]);
    setNonBlock(sp[1]);

    aeEventLoop *loop = aeCreateEventLoop();
    aeClient *client = aeClientCreate();
    TEST_ASSERT(client != NULL);
    TEST_ASSERT(aeCreateFileEvent(loop, sp[0], AE_READABLE, readQueryFromClient, client) == 0);

    unsigned char payload[1025];
    for (size_t i = 0; i < sizeof payload; ++i) payload[i] = (unsigned char)((i * 7) & 0xff);

    int er = echo_roundtrip(loop, sp[1], payload, sizeof payload, 512);
    TEST_ASSERT(er == 0);

    aeDeleteFileEvent(loop, sp[0], AE_READABLE | AE_WRITABLE);
    close(sp[0]);
    close(sp[1]);
    aeClientDestroy(client);
    aeDestroyEventLoop(loop);
}

static void test_add_writable_does_not_clobber_read_proc(void) {
    int sp[2];
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sp) == 0);
    setNonBlock(sp[0]);
    setNonBlock(sp[1]);

    aeEventLoop *loop = aeCreateEventLoop();
    aeClient *client = aeClientCreate();
    TEST_ASSERT(client != NULL);
    TEST_ASSERT(aeCreateFileEvent(loop, sp[0], AE_READABLE, readQueryFromClient, client) == 0);
    TEST_ASSERT(aeCreateFileEvent(loop, sp[0], AE_WRITABLE, sendReplyToClient, client) == 0);

    TEST_ASSERT(loop->events[sp[0]].readFileProc == readQueryFromClient);
    TEST_ASSERT(loop->events[sp[0]].writeFileProc == sendReplyToClient);

    aeDeleteFileEvent(loop, sp[0], AE_READABLE | AE_WRITABLE);
    close(sp[0]);
    close(sp[1]);
    aeClientDestroy(client);
    aeDestroyEventLoop(loop);
}

static void test_ae_client_alloc(void) {
    aeClient *c = aeClientCreate();
    TEST_ASSERT(c != NULL);
    TEST_ASSERT(c->outlen == 0 && c->outsent == 0);
    aeClientDestroy(c);
}

TEST_LIST = {
    {"create_destroy_and_poll", test_create_destroy_and_poll},
    {"fd_bounds", test_fd_bounds},
    {"delete_on_unregistered_fd", test_delete_on_unregistered_fd},
    {"set_nonblock_pipe", test_set_nonblock_pipe},
    {"pipe_readable_dispatch", test_pipe_readable_dispatch},
    {"socketpair_echo_small", test_socketpair_echo_small},
    {"socketpair_echo_1024_boundary", test_socketpair_echo_1024_boundary},
    {"socketpair_echo_1025_two_reads", test_socketpair_echo_1025_two_reads},
    {"add_writable_does_not_clobber_read_proc", test_add_writable_does_not_clobber_read_proc},
    {"ae_client_alloc", test_ae_client_alloc},
    {NULL, NULL},
};
