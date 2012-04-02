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

/* socket helper functions */
static int create_connection(char *, int, int);

char REMOTE_IP[] = "10.212.135.41";
#define REMOTE_IP_LEN (sizeof(REMOTE_IP) - 1)
#define REMOTE_PORT 10000

#define DATA_BUFF_LEN 256
char DATA_BUFF[DATA_BUFF_LEN];

#define TOTAL_TO_SEND 100000000


int main(int argc, char *argv[])
{
    int fd = create_connection(REMOTE_IP, REMOTE_IP_LEN, REMOTE_PORT);
    int amt, sent = 0;
    while(sent < TOTAL_TO_SEND) {
        amt = send(fd, DATA_BUFF, DATA_BUFF_LEN, 0);
        if(amt <= 0) break;
        sent += amt;
    }
    close(fd);
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

