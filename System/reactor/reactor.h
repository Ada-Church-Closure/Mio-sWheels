#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_EVENTS 1024
#define MAX_FDS 1024

// event mask
#define AE_NONE 0
#define AE_READABLE (1 << 0)
#define AE_WRITABLE (1 << 1)


// core structures
struct aeEventLoop;

// callback function
// when fd becomes readable or writable, it will be revoked.
typedef void aeFileProc(struct aeEventLoop *eventLoop, int fd, void* clientData, int mask);

// file event structure
typedef struct aeFileEvent {
    int mask; // this fd is readable or writable.
    aeFileProc *readFileProc; // callback functions
    aeFileProc *writeFileProc;
    void* clientData;
} aeFileEvent;

/* Per-connection output buffer (read fills, sendReplyToClient drains). */
typedef struct aeClient {
    unsigned char outbuf[16384];
    size_t outlen;
    size_t outsent;
} aeClient;

// event main loop
typedef struct aeEventLoop {
    int epollfd; // what is epoll's fd?
    aeFileEvent events[MAX_FDS]; // using fd as indexes
    struct epoll_event fired[MAX_EVENTS]; // the events that epoll_wait returned.
} aeEventLoop;

// the file must be non-block.
void setNonBlock(int fd);

// init aeEventLoop
aeEventLoop* aeCreateEventLoop();

void aeDestroyEventLoop(aeEventLoop* loop);

/* One epoll_wait + dispatch; timeout_ms -1 blocks, 0 polls. Returns count of ready fds, 0 on timeout, -1 on error. */
int aeProcessEvents(aeEventLoop* loop, int timeout_ms);

// register a file event into kernel epoll
int aeCreateFileEvent(aeEventLoop* loop, int fd, int mask, aeFileProc* fileProc, void* clientData);

// remove interest for `delmask` (AE_READABLE / AE_WRITABLE); updates epoll set
int aeDeleteFileEvent(aeEventLoop* loop, int fd, int delmask);

// event loop engine:for epoll wait to handle events.
void aeMain(aeEventLoop* eventLoop);

aeClient* aeClientCreate(void);
void aeClientDestroy(aeClient* c);

// read callback function, a client is ready, we can read from it.
void readQueryFromClient(struct aeEventLoop *eventLoop, int fd, void* clientData, int mask);

// accept a TCP handler.
void acceptTcpHandler(aeEventLoop *loop, int server_fd, void *clientData, int mask);

// send callback function, we can send something to client.
void sendReplyToClient(struct aeEventLoop *eventLoop, int fd, void* clientData, int mask);
