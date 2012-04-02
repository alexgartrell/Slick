obj-m := memcached.o
memcached-objs := frontend.o cache.o ../klibevent/klibevent.o ../specialsockets/bufferedsocket.o

#KDIR := /lib/modules/$(shell uname -r)/build
KDIR := /home/bblum/Code/mmotm/
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean
