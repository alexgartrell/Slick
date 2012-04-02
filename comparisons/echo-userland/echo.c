#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <fcntl.h>

#include <signal.h>
#include <errno.h>

#define check(A, M, ...) {                              \
        if(!(A)) {                                      \
            fprintf(stderr, "%s:%d: "M "\n",              \
                    __FILE__, __LINE__, ##__VA_ARGS__); \
            goto error;                                 \
        }                                               \
    }

#define DATA_PORT 12345

#define MAX_BUFF_SIZE (1 << 25)

static int create_listener(unsigned short);

static int recv_all(int fd, void *buff, int len, int flags);
static int send_all(int fd, void *buff, int len, int flags);

int main(int argc, char *argv[])
{
    int listener = -1;
    int fd = -1;
    int buff_size;
    char *buff = malloc(MAX_BUFF_SIZE);

    signal(SIGPIPE, SIG_IGN);

    listener = create_listener(DATA_PORT);
    check(listener >= 0, "create_listener failed");

    while((fd = accept(listener, NULL, NULL)) >= 0) {
        int amt;
        amt = recv(fd, &buff_size, sizeof(buff_size), 0);
        check(amt == sizeof(buff_size), "didn't receive whole int");
        buff_size = ntohl(buff_size);

        while((amt = recv_all(fd, buff, buff_size, 0)) > 0)
            send_all(fd, buff, amt, 0);
        close(fd); fd = -1;
    }

    close(listener); listener = -1;

    return 0;

error:
    if(listener >= 0) close(listener);
    if(fd >= 0) close(fd);
    return 1;
}

static int create_listener(unsigned short port)
{
    int fd = -1;
    int rcode = -1;
    int v = 1;
    struct sockaddr_in addr;
    check(port > 0, "Invalid port number: %u", port);

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    check(fd >= 0, "socket(...) failed");
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    v = 1;
    rcode = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
    check(rcode == 0, "setsockopt(...) failed");

    rcode = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
    check(rcode == 0, "bind(...) on port %d failed", port);

    rcode = listen(fd, 10);
    check(rcode == 0, "listen(...) failed");
    
    return fd;
error:
    perror("perror");

    if(fd >= 0) close(fd);
    return -1;
}

static int recv_all(int fd, void *buff, int len, int flags)
{
    int tot = 0;
    int amt;

    while(tot < len && (amt = recv(fd, buff + tot, len - tot, flags)) > 0)
        tot += amt;
    return tot;
}

static int send_all(int fd, void *buff, int len, int flags)
{
    int tot = 0;
    int amt;

    while(tot < len && (amt = send(fd, buff + tot, len - tot, flags)) > 0)
        tot += amt;
    return tot;
}
