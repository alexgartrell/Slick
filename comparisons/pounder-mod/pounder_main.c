#include <klibevent.h>
#include <dbg.h>

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/inet.h>
#include <linux/socket.h>
#include <net/sock.h>

char REMOTE_IP[] = "10.212.135.41";
#define REMOTE_IP_LEN (sizeof(REMOTE_IP) - 1)
#define REMOTE_PORT 10000

#define DATA_BUFF_LEN 256
char DATA_BUFF[DATA_BUFF_LEN];

#define TOTAL_TO_SEND 100000000

typedef struct Pounder {
    int sent;
    struct socket *sock;
} Pounder;

Pounder *my_pounder = NULL;

/* Util functions */
static int ghetto_pton(char *ip, int ip_len);

/* klibevent callbacks */
static void pounder_event(struct socket *sock, void *data, int event_flags);

static int ghetto_pton(char *ip, int ip_len)
{
    int i, d = 0, addr = 0;
    for(i = 0; i < ip_len && ip[i] != '\0'; i++) {
        if(ip[i] == '.') {
            addr = (addr << 8) | d;
            d = 0;
        }
        else
            d = (d * 10) + (ip[i] - '0');
    }
    addr = (addr << 8) | d;
    return ntohl(addr);
}


Pounder *create_connection(char *ip, int ip_len, int port)
{
    struct socket *csock = NULL;
    struct sockaddr_in addr;  
    int rcode;
    Pounder *pounder = NULL;

    printk("Attempting to connect to %.*s:%d\n", ip_len, ip, port);

    check(ip != NULL && ip_len > 0 && port > 0, 
          "Missing arguments to add_server");

    memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = ghetto_pton(ip, ip_len);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;

    rcode = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &csock);
    check(rcode == 0, "sock_create failed");

    rcode = csock->ops->connect(csock, (struct sockaddr *) &addr,
                                sizeof(addr), 0);
    check(rcode == 0, "connect failed with rcode %d", rcode);

    pounder = kmalloc(sizeof(*pounder), GFP_KERNEL);
    check(pounder != NULL, "failed to kmalloc ThriftSocket");
    pounder->sock = csock;
    pounder->sent = 0;

    rcode = register_sock(csock, pounder_event, pounder);
    check(rcode == 0, "register_sock failed");

    return pounder;

error:
    if(csock) {
        sock_release(csock);
        csock = NULL;
    }

    if(pounder != NULL) kfree(pounder);

    return NULL;
}

static void pounder_event(struct socket *sock, void *data, int event_flags)
{
    Pounder *pounder = (Pounder *) data;
    int amt = 0;

    if(event_flags & STATE_CHANGED) {
        if(TCP_SOCK_CLOSED(sock)) {
            unregister_sock(sock);
            sock_release(sock); sock = NULL;
            kfree(pounder); pounder = NULL; my_pounder = NULL;
            return;
        }
    }

    do {
        struct iovec iov;
        struct msghdr msg = {
            .msg_name = 0,
            .msg_namelen = 0,
            .msg_iov = &iov,
            .msg_iovlen = 1,
            .msg_control = NULL,
            .msg_controllen = 0,
            .msg_flags = MSG_DONTWAIT
        };

        if(pounder->sent >= TOTAL_TO_SEND) {
            unregister_sock(sock);
            sock_release(sock); sock = NULL;
            kfree(pounder); pounder = NULL; my_pounder = NULL;
            return;
        }
            
        
        iov.iov_base = DATA_BUFF;
        iov.iov_len = DATA_BUFF_LEN;

        amt = sock_sendmsg(sock, &msg, DATA_BUFF_LEN);
        if(amt > 0) pounder->sent += amt;
    } while(amt > 0);
    return;
}

static int __init init_pounder(void) {
    my_pounder = create_connection(REMOTE_IP, REMOTE_IP_LEN, REMOTE_PORT);
    check(my_pounder != NULL, "failed to create_connection");
    return 0;

error:
    return -1;
}

static void __exit exit_pounder(void) {
    if(my_pounder != NULL) {
        printk("Warning: Exiting while pounder still running!\n");
        sock_release(my_pounder->sock);
    }
}



module_init(init_pounder);
module_exit(exit_pounder);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Alex Gartrell");
MODULE_DESCRIPTION("Module to send a lot of data to one destination");


