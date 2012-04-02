obj-m := echo.o
echo-objs := echo_main.o ../../klibevent/klibevent.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
EXTRA_CFLAGS += -I$(PWD)/../../klibevent

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules
clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean