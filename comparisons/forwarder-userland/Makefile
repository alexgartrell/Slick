CFLAGS = -g
INCLUDES = -I.
LDFLAGS = -levent

forwarder: forwarder.o parser.o
	$(CC) $(CFLAGS) $(LDFLAGS) parser.o forwarder.o -o forwarder

%.o: %.c *.h Makefile
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ -c $<

clean:
	rm -rf *.o forwarder