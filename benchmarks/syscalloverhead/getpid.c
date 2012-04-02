#include <stdio.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>

#define ITERS 100000000

int fake_getpid() { return 13; }

int main(int argc, char *argv[])
{
    int i;
    struct timeval start, finish;
    double micros;

    int iters = (argc > 1) ? atoi(argv[1]) : ITERS;

#ifdef USERLAND
    printf("userland\n");
#else
    printf("kernel\n");
#endif

    gettimeofday(&start, NULL);
    for(i = 0; i < iters; ++i) {
#ifdef USERLAND
        fake_getpid();
#else
        syscall(__NR_getpid);
#endif

    }
    gettimeofday(&finish, NULL);
    
    micros = ((finish.tv_sec - start.tv_sec) * 1000000) +
        ((finish.tv_usec - start.tv_usec));
    printf("%d getpids @ %f nanoseconds per getpid\n", iters, micros / iters * 1000);
    return 0;
}
