CFLAGS = -g
INCLUDES = -I.

pounder: pounder.o
	$(CC) $(CFLAGS) pounder.o -o pounder

%.o: %.c *.h Makefile
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ -c $<

clean:
	rm -rf *.o pounder