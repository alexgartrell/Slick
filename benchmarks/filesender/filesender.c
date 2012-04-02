#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h> // getaddrinfo
#include <sys/stat.h> // open, stat
#include <fcntl.h> // open
#include <sys/socket.h> // socket, getaddrinfo
#include <netdb.h> // getaddrinfo

#include <unistd.h> // fork
#include <sys/wait.h> //wait

#define error_on(A) {                                                   \
        if(A) {                                                         \
            fprintf(stderr, "Error on %s:%d: %s\n",                     \
                    __FILE__, __LINE__, #A);                            \
            goto error;                                                 \
        }                                                               \
    }

void usage(char *prog)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "\t%s <nthreads> <host> <port> <iterations> <file>\n", prog);
    exit(1);
}

int read_file(char *path, char **contentsP)
{
    int fd = -1;
    int rc = -1;
    struct stat stat;
    char *contents = NULL;
    int amt, sofar;

    fd = open(path, O_RDONLY);
    error_on(fd < 0);

    rc = fstat(fd, &stat);
    error_on(rc != 0);
    
    contents = malloc(stat.st_size);
    error_on(contents == NULL);

    sofar = 0;
    while(sofar < stat.st_size) {
        amt = read(fd, contents + sofar, stat.st_size - sofar);
        error_on(amt <= 0);
        sofar += amt;
    }

    close(fd); fd = -1;
    *contentsP = contents;
    
    return stat.st_size;
error:
    perror("perror");
    if(fd > 0) close(fd);
    if(contents != NULL) free(contents);
    return -1;
}

int create_connection(char *host, char *service)
{
    int fd = -1;
    char my_ip[128];
    struct sockaddr_in addr;
    int rc = -1;
    struct addrinfo *ai = NULL;

    rc = getaddrinfo(host, service, NULL, &ai);
    error_on(rc != 0);

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    error_on(fd < 0);

    rc = connect(fd, ai->ai_addr, ai->ai_addrlen);
    error_on(rc != 0);

    freeaddrinfo(ai); ai = NULL;

    return fd;

error:
    perror("perror");
    if(fd >= 0) close(fd);
    if(ai != NULL) freeaddrinfo(ai);
    return -1;
}

int send_all(int fd, void *buf, int len)
{
    int sent = 0;
    while(sent < len) {
        int amt = send(fd, buf + sent, len - sent, 0);
        error_on(amt <= 0);
        sent += amt;
    }
    return sent;

error:
    perror("send");
    return -1;
}

struct thread_args {
    char *host;
    char *service;
    int iters;
    char *path;
};

void *run_thread(void *varg) {
    struct thread_args *args = (struct thread_args *) varg;
    int fd = -1;
    char *contents = NULL;
    int len;
    int i;

    len = read_file(args->path, &contents);
    error_on(len <= 0);

    fd = create_connection(args->host, args->service);
    error_on(fd < 0);
    
    for(i = 0; i < args->iters; i++) {
        int amt = send_all(fd, contents, len);
        error_on(amt <= 0);
    }

    close(fd); fd = -1;
    free(contents); contents = NULL;
    return NULL;
error:
    if(fd < 0) close(fd);
    return NULL;
}

int main(int argc, char *argv[])
{
    if(argc < 6) usage(argv[0]);
    int i;
    int nthreads = atoi(argv[1]);
    pthread_t threads[nthreads];

    struct thread_args args = {
        .host = argv[2],
        .service = argv[3],
        .iters = atoi(argv[4]),
        .path = argv[5]
    };

    for(i = 0; i < nthreads; i++) {
        if(fork() == 0) {
            run_thread(&args);
            exit(0);
        }
    }
    for(i = 0; i < nthreads; i++)
        wait(NULL);
    return 0;

error:
    return 1;
}
