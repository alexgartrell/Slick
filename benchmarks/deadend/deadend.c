#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <atomic.h>
#include <pthread.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFF_SIZE (1 << 20)

#define check(A, M, ...) {                           \
        if(!(A)) {                                   \
            fprintf(stderr, M "\n", ##__VA_ARGS__);  \
            goto error;                              \
        }                                            \
    }

struct serve_args {
    unsigned short port;
};

static atomic_t total_amt;

void add_received(ssize_t amt)
{
    atomic_add(&total_amt, amt);
}

ssize_t get_received()
{
    return atomic_xchg(&total_amt, 0);
}

void record()
{
    struct timeval start, end;
    ssize_t amt;
    long long int diff;
    double bps, last_bps = 0, total_bps = 0, first_bps;
    int total_adds = 0;
    int trial = 1;

    gettimeofday(&start, NULL);
    while(1) {
        usleep(1000000);
        gettimeofday(&end, NULL);
        amt = get_received();

        diff = (end.tv_sec * 1000000 + end.tv_usec) - 
            (start.tv_sec * 1000000 + start.tv_usec);
        bps = (amt * 8 * 1000000) / diff;

        /* Beginning */
        if(last_bps == 0.0 && bps != 0.0) {
            first_bps = bps;
        }
        /* Any of them in the middle */
        if(last_bps != 0.0 && bps != 0.0) {
            total_bps += bps;
            total_adds++;
        }
        /* Just got the last one */
        else if(last_bps != 0.0 && bps == 0.0) {
            if(total_adds <= 1) {
                printf("*One or Two Intervals -- Including edges*\n");
            }
            else {
                total_bps -= last_bps;
                total_adds--;
                if(total_adds != 0)
                    printf("%.2f\t%f\n", total_bps / total_adds,
                           (total_bps / total_adds) / 1000000);
            }
            trial++;
            total_bps = 0.0;
            total_adds = 0;
            first_bps = 0.0;
        }

        last_bps = bps;
        start = end;
    }
}

void serve(unsigned short port)
{
    int listener = -1;
    int fd = -1;
    int rcode;
    ssize_t amt;
    struct sockaddr_in addr;
    char *buff = NULL;
    int x;

    buff = malloc(BUFF_SIZE);
    assert(buff != NULL);

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    check(listener >= 0, "socket(...) failed");
    
    x = 1;
    rcode = setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &x, sizeof(x));
    check(rcode == 0, "setsockopt(...) failed");

    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    rcode = bind(listener, (struct sockaddr *) &addr, sizeof(addr));
    check(rcode == 0, "bind(...) to port %d failed", port);

    rcode = listen(listener, 10);
    check(rcode == 0, "listen(...) failed");
    while((fd = accept(listener, NULL, NULL)) >= 0) {
        while((amt = recv(fd, buff, BUFF_SIZE, 0)) > 0) {
            add_received(amt);
        }
        close(fd);
    }

    /* intentional fall-through */
error:
    if(listener >= 0) close(listener);
}

void *launch_serve(void *data)
{
    struct serve_args *sa = (struct serve_args *) data;
    check(sa != NULL, "NULL argument to launch_serve, shouldn't happen");
    serve(sa->port);

    /* fall-through, because serve won't return */
error:
    exit(1);
    return NULL;
}

int main(int argc, char *argv[])
{
    int rcode;
    int i;
    int port_start, num_ports;
    pthread_t thread;

    check(argc == 3, "Too few/many arguments, expected\n"
          "\tdeadend <port_start> <num_ports>");
    port_start = atoi(argv[1]);
    num_ports = atoi(argv[2]);

    fflush(stdout);
    for(i = 0; i < num_ports; i++) {
        struct serve_args *sa = malloc(sizeof(*sa));
        check(sa != NULL, "malloc failed");
        sa->port = port_start + i;

        rcode = pthread_create(&thread, NULL, launch_serve, sa);
        check(rcode == 0, "pthread_create failed");
        pthread_detach(thread);
    }

    // Start recording throughput
    record();

    return 0;
error:
    return 1;
}
