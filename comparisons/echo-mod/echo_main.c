#include <klibevent.h>

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/socket.h>
#include <net/sock.h>

// #include <linux/slab.h>

#define PORT 12345
#define BACKLOG_SIZE 10

#define MAX_BUFF_SIZE 131048
char *buff = NULL;

struct socket *listen_sock;

static int send(struct socket *sock, void *buff, int len, int flags)
{
    struct iovec iov;
    struct msghdr msg = {
        .msg_name = 0,
        .msg_namelen = 0,
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = NULL,
        .msg_controllen = 0,
        .msg_flags = flags
    };

    iov.iov_base = buff;
    iov.iov_len = len;

    return sock_sendmsg(sock, &msg, len);
}

static int recv(struct socket *sock, void *buff, int len, int flags)
{
    struct iovec iov;
    struct msghdr msg = {
        .msg_name = 0,
        .msg_namelen = 0,
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = NULL,
        .msg_controllen = 0,
        .msg_flags = flags
    };

    iov.iov_base = buff;
    iov.iov_len = len;

    return sock_recvmsg(sock, &msg, len, flags);
}

static void listen_event(struct socket *lsock, void *data, int event_flags) {
    struct socket *csock;
    int error;
    int amt;
    int buff_size;

    if(event_flags & DATA_READY) {
        while(1) {
            error = sock_create_lite(PF_INET, SOCK_STREAM, IPPROTO_TCP, &csock);
            if(error) return;
            csock->ops = lsock->ops;
            error = lsock->ops->accept(lsock, csock, O_NONBLOCK);
            if(error) break;

            if(recv(csock, &buff_size, sizeof(buff_size), 0) 
               == sizeof(buff_size) && ntohl(buff_size) <= MAX_BUFF_SIZE) {
                buff_size = ntohl(buff_size);
                while((amt = recv(csock, buff, buff_size, 0)) > 0)
                    send(csock, buff, amt, 0);
            }
            sock_release(csock);
        }
        sock_release(csock);
    }
}

static int __init init_echo(void) {
    struct sockaddr_in myaddr;  
    int error = 0;
  
    buff = kmalloc(MAX_BUFF_SIZE, GFP_KERNEL);
    if(buff == NULL) {
        printk("Kmalloc failed\n");
        goto fail;
    }

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
    kfree(buff); buff = NULL;
}

module_init(init_echo);
module_exit(exit_echo);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Alex Gartrell");
MODULE_DESCRIPTION("Echo Module");


