all: userland_getpid kernel_getpid

userland_getpid: getpid.c
	$(CC) $(CFLAGS) -DUSERLAND getpid.c -o userland_getpid
kernel_getpid: getpid.c
	$(CC) $(CFLAGS) getpid.c -o kernel_getpid

clean:
	rm -rf userland_getpid kernel_getpid