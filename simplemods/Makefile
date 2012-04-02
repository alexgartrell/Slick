obj-m := echo.o static_router.o print.o
echo-objs := echo_main.o ../klibevent/klibevent.o \
	../specialsockets/bufferedsocket.o
print-objs := print_main.o ../klibevent/klibevent.o \
	../specialsockets/bufferedsocket.o
static_router-objs := static_router_main.o ../klibevent/klibevent.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
EXTRA_CFLAGS += -I$(PWD)/../klibevent -I$(PWD)/../specialsockets

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules
clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean