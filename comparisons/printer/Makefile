obj-m := thrift_print.o

KLIBEVENT_OBJS := ../../klibevent/klibevent.o \
	../../specialsockets/bufferedsocket.o \
	../../specialsockets/thriftsocket.o
thrift_print-objs := thrift_print_main.o $(KLIBEVENT_OBJS)

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
EXTRA_CFLAGS += -I$(PWD)/../../klibevent -I$(PWD)/../../specialsockets -I$(PWD)

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules
clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean