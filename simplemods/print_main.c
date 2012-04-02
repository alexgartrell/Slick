#include "../klibevent/klibevent.h"
#include "../specialsockets/bufferedsocket.h"
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/socket.h>
#include <net/sock.h>

// #include <linux/slab.h>

#define PORT 12345
#define BACKLOG_SIZE 10

struct socket *listen_sock;

static void client_event(BufferedSocket *bs, void *data, int event_flags) {
    printk("Event on client sock %ld = 0x%X\n", (long) data, event_flags);

    if(event_flags & STATE_CHANGED) {
        if(TCP_SOCK_CLOSED(bs->sock)) {
            BufferedSocket_cleanup(bs);
            kfree(bs);
            return;
        }
    }

    if((event_flags & DATA_READY)) {
        int len;
        char *p;
        BufferedSocket_reset_buffer(bs);
        while((len = BufferedSocket_peek(bs, &p)) > 20) {
            printk("%.*s", len, p);
            BufferedSocket_discard(bs, len);
        }
    }

}

static void listen_event(struct socket *lsock, void *data, int event_flags) {
    static int client_counter = 0;
    struct socket *csock;
    int error;
    BufferedSocket *bs;

    if(event_flags & DATA_READY) {
        while(1) {
            error = sock_create_lite(PF_INET, SOCK_STREAM, IPPROTO_TCP, &csock);
            if(error) return;
            csock->ops = lsock->ops;
            error = lsock->ops->accept(lsock, csock, O_NONBLOCK);
            if(error) break;
            
            bs = kmalloc(sizeof(*bs), GFP_KERNEL);
            if(bs == NULL) break;

            BufferedSocket_init(bs, csock, client_event,
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
MODULE_DESCRIPTION("echo to printk Module");


