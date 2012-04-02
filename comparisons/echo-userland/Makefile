CFLAGS = -g
INCLUDES = -I.
LDFLAGS =

echo: echo.o
	$(CC) $(CFLAGS) $(LDFLAGS) echo.o -o echo

%.o: %.c *.h Makefile
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ -c $<

clean:
	rm -rf *.o echo