CFLAGS = -Wall -Wextra -I. -g
LDFLAGS = -lpthread

deadend: deadend.o atomic.o
	$(CC) $(LDFLAGS) $(CFLAGS) deadend.o atomic.o -o deadend

clean:
	rm -rf *.o deadend

deadend.o: deadend.c atomic.h
	$(CC) $(CFLAGS) -c deadend.c
atomic.o: atomic.S
	$(CC) $(CFLAGS) -c atomic.S
