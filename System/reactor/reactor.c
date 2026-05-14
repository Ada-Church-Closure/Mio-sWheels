#include "reactor.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

void aeClientDestroy(aeClient* c) { free(c); }

aeClient* aeClientCreate(void) {
    aeClient* c = calloc(1, sizeof(aeClient));
    return c;
}

void setNonBlock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL O_NONBLOCK");
    }
}

aeEventLoop* aeCreateEventLoop () {
    aeEventLoop* loop = malloc(sizeof(aeEventLoop));
    if (!loop) {
        perror("create event loop failed.");
        exit(1);
    }    

    loop->epollfd = epoll_create1(0);
    if (loop->epollfd == -1) {
        perror("create epollfd failed");
        exit(1);
    }

    for (int index = 0; index < MAX_FDS; ++index) {
        loop->events[index].mask = AE_NONE;
        loop->events[index].readFileProc = NULL;
        loop->events[index].writeFileProc = NULL;
        loop->events[index].clientData = NULL;
    }
    return loop;
}

void aeDestroyEventLoop(aeEventLoop* loop) {
    if (!loop) return;
    close(loop->epollfd);
    free(loop);
}

static void aeDispatchEvents(aeEventLoop* eventLoop, int numevents) {
    for (int index = 0; index < numevents; ++index) {
        struct epoll_event ee = eventLoop->fired[index];
        int fd = ee.data.fd;
        aeFileEvent* fe = &eventLoop->events[fd];

        int mask = 0;
        if (ee.events & EPOLLIN) mask |= AE_READABLE;
        if (ee.events & EPOLLOUT) mask |= AE_WRITABLE;
        if (ee.events & EPOLLERR || ee.events & EPOLLHUP) mask |= AE_READABLE | AE_WRITABLE;

        int rfired = 0;
        if ((fe->mask & mask & AE_READABLE) && fe->readFileProc) {
            rfired = 1;
            fe->readFileProc(eventLoop, fd, fe->clientData, AE_READABLE);
        }

        if ((fe->mask & mask & AE_WRITABLE) && fe->writeFileProc) {
            if (!rfired || fe->readFileProc != fe->writeFileProc) {
                fe->writeFileProc(eventLoop, fd, fe->clientData, AE_WRITABLE);
            }
        }
    }
}

int aeProcessEvents(aeEventLoop* eventLoop, int timeout_ms) {
    int numevents = epoll_wait(eventLoop->epollfd, eventLoop->fired, MAX_EVENTS, timeout_ms);
    if (numevents < 0) {
        if (errno == EINTR) return 0;
        return -1;
    }
    aeDispatchEvents(eventLoop, numevents);
    return numevents;
}

int aeCreateFileEvent(aeEventLoop* loop, int fd, int mask, aeFileProc* fileProc, void* clientData) {
    if (fd < 0 || fd >= MAX_FDS) return -1;
    
    aeFileEvent* fe = &loop->events[fd];

    int epollOp = fe->mask == AE_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;

    fe->mask |= mask;
    /* Only update the callback for the mask bits being registered (Redis aeCreateFileEvent). */
    if (mask & AE_READABLE) fe->readFileProc = fileProc;
    if (mask & AE_WRITABLE) fe->writeFileProc = fileProc;
    fe->clientData = clientData;

    struct epoll_event ee = {0};

    /* default: level-triggered; OR both directions when registered */
    if (fe->mask & AE_READABLE) ee.events |= EPOLLIN;
    if (fe->mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;

    // register a file event.
    if (epoll_ctl(loop->epollfd, epollOp, fd, &ee) == -1) {
        perror("register epoll event failed.");
        return -1;
    }
    return 0;
}

int aeDeleteFileEvent(aeEventLoop* loop, int fd, int delmask) {
    if (fd < 0 || fd >= MAX_FDS) return -1;

    aeFileEvent* fe = &loop->events[fd];
    if (fe->mask == AE_NONE) return 0;

    fe->mask &= ~delmask;

    if (fe->mask == AE_NONE) {
        if (epoll_ctl(loop->epollfd, EPOLL_CTL_DEL, fd, NULL) == -1 && errno != ENOENT && errno != EBADF) {
            perror("epoll_ctl DEL");
            return -1;
        }
        return 0;
    }

    struct epoll_event ee = {0};
    if (fe->mask & AE_READABLE) ee.events |= EPOLLIN;
    if (fe->mask & AE_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;

    if (epoll_ctl(loop->epollfd, EPOLL_CTL_MOD, fd, &ee) == -1) {
        perror("epoll_ctl MOD");
        return -1;
    }
    return 0;
}

void aeMain(aeEventLoop *eventLoop) {
    for (;;) {
        if (aeProcessEvents(eventLoop, -1) < 0) {
            perror("aeProcessEvents");
            break;
        }
    }
}

static void freeClientConnection(aeEventLoop* eventLoop, int fd, aeClient* client) {
    aeDeleteFileEvent(eventLoop, fd, AE_READABLE | AE_WRITABLE);
    close(fd);
    eventLoop->events[fd].mask = AE_NONE;
    eventLoop->events[fd].readFileProc = NULL;
    eventLoop->events[fd].writeFileProc = NULL;
    eventLoop->events[fd].clientData = NULL;
    aeClientDestroy(client);
}

void readQueryFromClient(struct aeEventLoop *eventLoop, int fd, void* clientData, int mask) {
    (void)mask;
    aeClient* client = (aeClient*)clientData;
    if (!client) return;

    /* Do not read new requests until the previous reply is fully sent (simple single-buffer model). */
    if (client->outsent < client->outlen) return;

    char buf[1024];
    int nread = read(fd, buf, sizeof(buf));

    if (nread == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        perror("read error");
        freeClientConnection(eventLoop, fd, client);
        return;
    }
    if (nread == 0) {
        freeClientConnection(eventLoop, fd, client);
        return;
    }

    memcpy(client->outbuf, buf, (size_t)nread);
    client->outlen = (size_t)nread;
    client->outsent = 0;

    /* Schedule non-blocking send on writable (Redis: enableWriteEventIfNeeded). */
    if (aeCreateFileEvent(eventLoop, fd, AE_WRITABLE, sendReplyToClient, client) == -1) {
        freeClientConnection(eventLoop, fd, client);
    }
}

void acceptTcpHandler(aeEventLoop *loop, int server_fd, void *clientData, int mask) {
    (void)clientData;
    (void)mask;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) perror("accept error");
        return;
    }
    
    setNonBlock(client_fd);

    aeClient* client = aeClientCreate();
    if (!client) {
        perror("aeClientCreate");
        close(client_fd);
        return;
    }

    if (aeCreateFileEvent(loop, client_fd, AE_READABLE, readQueryFromClient, client) == -1) {
        aeClientDestroy(client);
        close(client_fd);
        return;
    }
}

void sendReplyToClient(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
    (void)mask;
    aeClient* client = (aeClient*)clientData;
    if (!client || client->outsent >= client->outlen) {
        aeDeleteFileEvent(eventLoop, fd, AE_WRITABLE);
        return;
    }

    const unsigned char* p = client->outbuf + client->outsent;
    size_t left = client->outlen - client->outsent;
    ssize_t nwritten = write(fd, p, left);

    if (nwritten == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        perror("write error");
        freeClientConnection(eventLoop, fd, client);
        return;
    }

    client->outsent += (size_t)nwritten;

    if (client->outsent == client->outlen) {
        client->outlen = 0;
        client->outsent = 0;
        aeDeleteFileEvent(eventLoop, fd, AE_WRITABLE);
    }
}

