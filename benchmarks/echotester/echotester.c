#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <fcntl.h>

#include <signal.h>
#include <errno.h>
#include <sys/time.h>

#define check(A, M, ...) {                              \
        if(!(A)) {                                      \
            fprintf(stderr, "%s:%d: "M "\n",              \
                    __FILE__, __LINE__, ##__VA_ARGS__); \
            goto error;                                 \
        }                                               \
    }

char IP[] = "10.212.135.43";
#define IP_LEN (sizeof(IP) - 1)
#define PORT 12345
#define BUFF_SIZE 512
#define TO_SEND (1 << 26)
#define BUFF_SIZE_START 32
#define TRIALS 10
#define RUNS_PER_TRIAL 5

long long average_latency_us;
long long average_throughput_Bps;

/* socket helper functions */
static int create_connection(char *, int, int);


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

static long long diff_time(struct timeval *start, struct timeval *end)
{
    return (end->tv_sec - start->tv_sec) * 1000000 
            + (end->tv_usec - start->tv_usec);
}


static void *sender_func(void *data)
{
    int fd = *(int *) data;
    char *buff = malloc(BUFF_SIZE);
    assert(buff != NULL);
    int amt;
    int total = 0;
    struct timeval start;

    while(total < TO_SEND) {
        gettimeofday(&start, NULL);
        amt = send_all(fd, &start, sizeof(start), 0);
        if(amt != sizeof(start))
            break;

        total += amt;
        amt = send_all(fd, buff, BUFF_SIZE, 0);
        if(amt != BUFF_SIZE)
            break;
        total += amt;
    }
    return NULL;
}

static void *receiver_func(void *data) {
    int fd = *(int *) data;
    char *buff = malloc(BUFF_SIZE);
    assert(buff != NULL);
    struct timeval start;
    struct timeval end;
    long long diff;

    long long total_latency = 0;
    int trials = 0;
    struct timeval total_start;
    struct timeval total_end;

    gettimeofday(&total_start, NULL);
    while(1) {
        if(recv_all(fd, &start, sizeof(start), 0) != sizeof(start))
            break;
        gettimeofday(&end, NULL);

        if(recv_all(fd, buff, BUFF_SIZE, 0) != BUFF_SIZE)
            break;
        trials++;
        total_latency += diff_time(&start, &end);
    }
    gettimeofday(&total_end, NULL);

    average_latency_us = total_latency / trials;
    average_throughput_Bps = 
        ((long long) (BUFF_SIZE + sizeof(start)) * trials)
        / ((long double) diff_time(&total_start, &total_end) / 1000000);
    return NULL;
}

static int run_test(int buff_size, long long *throughput, long long *latency) {
    int fd = -1;
    int amt, x;
    pthread_t sender, receiver;


    fd = create_connection(IP, IP_LEN, PORT);
    check(fd >= 0, "create_connection failed");
    
    x = htonl(buff_size);
    amt = send_all(fd, &x, sizeof(x), 0);
    check(amt == sizeof(x), "Failed to send all of buffer size");

    pthread_create(&receiver, NULL, receiver_func, &fd);
    pthread_create(&sender, NULL, sender_func, &fd);

    pthread_join(sender, NULL);
    close(fd);

    pthread_join(receiver, NULL);

    *throughput = average_throughput_Bps;
    *latency = average_latency_us;

    return 0;
error:
    if(fd > 0) close(fd);
    fd = -1;
    return 1;

}

int main(int argc, char *argv[])
{
    int i, j;
    for(i = 0; i < TRIALS; i++) {
        int buff_size = BUFF_SIZE_START << i;

        long long average_throughput = 0;
        long long average_latency = 0;
        for(j = 0; j < RUNS_PER_TRIAL; j++) {
            long long throughput, latency;
            int rcode = run_test(buff_size, &throughput, &latency);
            check(rcode == 0, "Failed to run_test?");

            average_throughput += throughput;
            average_latency += latency;
        }
        average_throughput /= RUNS_PER_TRIAL;
        average_latency /= RUNS_PER_TRIAL;
        printf("%d\t%f\t%lld\n", buff_size, 
               ((double) average_throughput * 8) / 1000000, average_latency);
    }

    return 0;
error:
    return 1;
}


static int create_connection(char *ip, int ip_len, int port)
{
    int fd = -1;
    char my_ip[128];
    struct sockaddr_in addr;
    int rcode = -1;

    check(sizeof(my_ip) - 1 >= ip_len, "unreasonably long ip_len %d", ip_len);
    memcpy(my_ip, ip, ip_len);
    my_ip[ip_len] = '\0';

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    rcode = inet_pton(AF_INET, my_ip, &addr.sin_addr);
    check(rcode == 1, "inet_pton(...) failed");
    addr.sin_port = htons(port);

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    check(fd >= 0, "socket(...) failed");

    rcode = connect(fd, (struct sockaddr *) &addr, sizeof(addr));
    check(rcode == 0, "connect(...) failed");

    return fd;

error:
    perror("perror");
    if(fd >= 0) close(fd);
    return -1;
}

