#include <klibevent.h>
#include <thriftsocket.h>
#include <bufferedsocket.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/socket.h>
#include <net/sock.h>

// #include <linux/slab.h>

#define PORT 12345
#define BACKLOG_SIZE 10

struct socket *listen_sock;

static void client_event(ThriftSocket *ts, void *data, int event_flags) {
    ThriftMessage tm;
    int i;

    if(event_flags & STATE_CHANGED) {
        if(TCP_SOCK_CLOSED(ts->bs.sock)) {
            ThriftSocket_cleanup(ts);
            kfree(ts);
            return;
        }
    }

    if((event_flags & DATA_READY)) {
        while(ThriftSocket_next(ts, &tm) == 1) {
            printk("messageType: %d\n", tm.messageType);
            printk("name_len: %d\n", tm.name_len);
            printk("name: %.*s\n", tm.name_len, tm.name_ptr);
            printk("seqid: %d\n", tm.seqid);
            printk("num fields: %d\n", tm.fields_len);
            for(i = 0; i < tm.fields_len; i++) {
                printk("fields[%d].fieldType = %d\n", i,
                       tm.fields[i].fieldType);
                printk("fields[%d].fieldId = %d\n", i,
                       tm.fields[i].fieldId);
                printk("fields[%d].val_len = %d\n", i,
                       tm.fields[i].val_len);
                printk("fields[%d].val_ptr = %p\n", i,
                       tm.fields[i].val_ptr);
            }
            
            ThriftSocket_discard(ts);
        }
    }

}

static void listen_event(struct socket *lsock, void *data, int event_flags) {
    static int client_counter = 0;
    struct socket *csock;
    int error;
    ThriftSocket *ts;

    if(event_flags & DATA_READY) {
        while(1) {
            error = sock_create_lite(PF_INET, SOCK_STREAM, IPPROTO_TCP, &csock);
            if(error) return;
            csock->ops = lsock->ops;
            error = lsock->ops->accept(lsock, csock, O_NONBLOCK);
            if(error) break;

            ts = kmalloc(sizeof(*ts), GFP_KERNEL);
            if(ts == NULL) break;

            ThriftSocket_init(ts, csock, client_event,
                              (void *) (long) client_counter);
            client_counter++;
        }
        sock_release(csock);
    }
}

static int __init init_print(void) {
    struct sockaddr_in myaddr;  
    int error;
  
    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = htons(PORT);
    myaddr.sin_family = AF_INET;

    error = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &listen_sock);
    if(error) goto fail;
    error = listen_sock->ops->bind(listen_sock, (struct sockaddr*) &myaddr,
                                   sizeof(myaddr));
    if(error) goto fail;
    error = listen_sock->ops->listen(listen_sock, BACKLOG_SIZE);
    if(error) goto fail;
  
    register_sock(listen_sock, listen_event, NULL);

    return 0;

fail:
    if(listen_sock != NULL)
        sock_release(listen_sock);
    listen_sock = NULL;
    return error;
}

static void __exit exit_print(void) {
    if(listen_sock != NULL) {
        unregister_sock(listen_sock);
        sock_release(listen_sock);
    }
    listen_sock = NULL;
}



module_init(init_print);
module_exit(exit_print);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Alex Gartrell");
MODULE_DESCRIPTION("echo thrift call to printk Module");


