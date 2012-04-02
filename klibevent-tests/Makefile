CC = gcc
CFLAGS = -g

test: circbuffer-tests
	./circbuffer-tests
circbuffer-tests: circbuffer-tests.o ../klibevent/circbuffer.h
	$(CC) $(CFLAGS) circbuffer-tests.o -o circbuffer-tests
%.o: $*.c
	$(CC) $(CFLAGS) -c $*.c

clean:
	rm *~ *.o circbuffer-tests