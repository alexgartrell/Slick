#include "../klibevent/klibevent.h"

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/socket.h>
#include <net/sock.h>

// #include <linux/slab.h>

#define PORT 12345
#define BACKLOG_SIZE 10
#define BUFFER_LEN 1024

#define REMOTE_PORT 10001
//#define REMOTE_IP 0x7f000001
#define REMOTE_IP 0x0AD4872A

struct client_data {
    int id;
    char buffer[BUFFER_LEN];
    int off;
    int len;
    struct socket *dest_sock;
    struct client_data *other;
};

struct socket *listen_sock;

static void client_event(struct socket *sock, void *data, int event_flags) {
    struct client_data *cd;
    struct msghdr msg;
    struct iovec iov;
    int len;
    cd = (struct client_data*) data;

    printk("Event on client sock %d = 0x%X\n", cd->id, event_flags);

    if((event_flags & DATA_READY) || (event_flags & WRITE_SPACE)) {
        msg.msg_name = 0;
        msg.msg_namelen = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags = MSG_DONTWAIT;
    
        iov.iov_base = cd->buffer;
        iov.iov_len = BUFFER_LEN;
    
        /* send old stuff */
        while(cd->len - cd->off > 0) {
            iov.iov_base = cd->buffer + cd->off;
            iov.iov_len = cd->len - cd->off;
            len = sock_sendmsg(cd->dest_sock, &msg, cd->len - cd->off);
            if(len <= 0) goto done;
            cd->off += len;
        }
    
        /* recieve everything from the queue */
        while(1) {
            iov.iov_base = cd->buffer;
            iov.iov_len = BUFFER_LEN;
            cd->len = sock_recvmsg(sock, &msg, BUFFER_LEN, MSG_DONTWAIT);
            cd->off = 0;
            if(cd->len <= 0) {
                cd->len = 0;
                goto done;
            }
            /* send everything from the queue */
            while(cd->len - cd->off > 0) {
                iov.iov_base = cd->buffer + cd->off;
                iov.iov_len = cd->len - cd->off;
                len = sock_sendmsg(cd->dest_sock, &msg, cd->len - cd->off);
                if(len <= 0) goto done;
                cd->off += len;
            }
        }
    }

 done:
    if(event_flags & STATE_CHANGED) {
        if(TCP_SOCK_CLOSED(sock)) {
            unregister_sock(sock);
            sock_release(sock);
            unregister_sock(cd->dest_sock);
            sock_release(cd->dest_sock);
            kfree(cd);
            kfree(cd->other);
        }
    }
    return;
}

static void listen_event(struct socket *lsock, void *data, int event_flags) {
    static int client_counter = 0;
    struct socket *csock = NULL;
    struct socket *dsock = NULL;
    int error;
    struct client_data *cd1;
    struct client_data *cd2;

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(REMOTE_PORT),
        .sin_addr = {
            .s_addr = htonl(REMOTE_IP)
        }
    };


    if(event_flags & DATA_READY) {
        while(1) {
            error = sock_create_lite(PF_INET, SOCK_STREAM, IPPROTO_TCP, &csock);
            if(error) return;
            csock->ops = lsock->ops;
            error = lsock->ops->accept(lsock, csock, O_NONBLOCK);
	    printk("after accept: error %d\n", error);
            if(error) break;

            error = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &dsock);
            if(error) break;
            error = dsock->ops->connect(dsock, (struct sockaddr*) &addr,
                                        sizeof(addr), 0);
            if(error) break;

            cd1 = kmalloc(sizeof(*cd1), GFP_KERNEL);
            if(cd1 == NULL) break;

            cd1->dest_sock = dsock;
            cd1->id = client_counter++;
            cd1->len = cd1->off = 0;
            register_sock(csock, client_event, cd1);
	    printk("registered 1\n");            
            cd2 = kmalloc(sizeof(*cd2), GFP_KERNEL);
            if(cd2 == NULL) break; // memory leak
            cd2->dest_sock = csock;
            cd2->id = client_counter++;
            cd2->len = cd2->off = 0;
            register_sock(dsock, client_event, cd2);
	    printk("registered 2\n");
            cd1->other = cd2;
            cd2->other = cd1;
	    printk("done\n");
            // Clear our references because we succeeded and don't want to
            // prematurely clean them up below
            dsock = NULL;
            csock = NULL;
            cd1 = NULL;
            cd2 = NULL;
        }

        if(csock)
            sock_release(csock);
        if(dsock)
            sock_release(dsock);
        if(cd1)
            kfree(cd1);
        if(cd2)
            kfree(cd2);
    }
}

static int __init init_echo(void) {
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

static void __exit exit_echo(void) {
    if(listen_sock != NULL) {
        unregister_sock(listen_sock);
        sock_release(listen_sock);
    }
    listen_sock = NULL;
}



module_init(init_echo);
module_exit(exit_echo);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Alex Gartrell");
MODULE_DESCRIPTION("Echo Module");


