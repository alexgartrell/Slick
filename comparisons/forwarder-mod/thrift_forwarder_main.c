#include <klibevent.h>
#include <thriftsocket.h>
#include <bufferedsocket.h>
#include <dbg.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>

#include <linux/inet.h>
#include <linux/socket.h>
#include <net/sock.h>

#define CONTROLLER_LISTEN_PORT 54321
#define CLIENT_LISTEN_PORT 12345
#define BACKLOG_SIZE 10

#define KEY_ID 1

struct socket *controller_listener;
struct socket *client_listener;

typedef struct Server {
    int key;
    ThriftSocket ts;
    struct list_head server_list;
} Server;

LIST_HEAD(server_list);

typedef struct Client {
    ThriftSocket ts;
} Client;

#define CLIENT_LISTENER  ((void *) 1)
#define CONTROLLER_LISTENER ((void *) 2)

/* Util functions */
static int ghetto_pton(char *ip, int ip_len);

/* RPC Calls */
static void add_server(char *ip, int ip_len, int port, int key);
static void release_servers(void);

/* ThriftSocket callbacks */
static void controller_event(ThriftSocket *ts, void *data, int event_flags);
static void server_event(ThriftSocket *ts, void *data, int event_flags);
static void client_event(ThriftSocket *ts, void *data, int event_flags);

/* klibevent callbacks */
static void client_listener_event(struct socket *lsock, void *data,
                                  int event_flags);
static void controller_listener_event(struct socket *lsock, void *data,
                                      int event_flags);

/* server list locking */
static void write_lock_server_list(void);
static void read_lock_server_list(void);
static void unlock_server_list(void);

static int ghetto_pton(char *ip, int ip_len)
{
    int i, d = 0, addr = 0;
    for(i = 0; i < ip_len; i++) {
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

static void write_lock_server_list()
{
    /* Nop */
}

static void read_lock_server_list()
{
    /* Nop */
}

static void unlock_server_list()
{
    /* Nop */
}

static void add_server(char *ip, int ip_len, int port, int key)
{
    struct socket *csock = NULL;
    struct sockaddr_in addr;  
    int rcode;
    Server *server = NULL;

    check(ip != NULL && ip_len > 0 && port > 0, 
          "Missing arguments to add_server");

    printk("add_server(ip=%.*s, port=%d, key=%d)\n", ip_len, ip, port, key);

    memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = ghetto_pton(ip, ip_len);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;

    rcode = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &csock);
    check(rcode == 0, "sock_create failed");

    rcode = csock->ops->connect(csock, (struct sockaddr *) &addr,
                                sizeof(addr), 0);
    check(rcode == 0, "connect failed with rcode %d", rcode);

    server = kmalloc(sizeof(*server), GFP_KERNEL);
    check(server != NULL, "failed to kmalloc ThriftSocket");

    rcode = ThriftSocket_init(&server->ts, csock, server_event, server);
    check(rcode == 0, "ThriftSocket_init failed");
    server->key = key;

    write_lock_server_list();
    list_add(&server->server_list, &server_list);
    unlock_server_list();
    
    return;

error:
    if(csock) {
        sock_release(csock);
        csock = NULL;
    }
    // We aren't ThriftSocket_destroying here, because there's no need yet
    if(server != NULL) kfree(server);

    return;
}

static void release_servers(void)
{
    Server *pos, *n;

    printk("release_servers()\n");

    write_lock_server_list();
    list_for_each_entry_safe(pos, n, &server_list, server_list) {
        ThriftSocket_cleanup(&pos->ts);
        list_del(&pos->server_list);
        kfree(pos);
    }
    unlock_server_list();
}

static void controller_event(ThriftSocket *ts, void *data, int event_flags)
{
    ThriftMessage tm;

    if(event_flags & STATE_CHANGED) {
        if(TCP_SOCK_CLOSED(ts->bs.sock)) {
            ThriftSocket_cleanup(ts);
            kfree(ts);
            return;
        }
    }

    if((event_flags & DATA_READY || event_flags & WRITE_SPACE)) {
        while(ThriftSocket_next(ts, &tm) == 1) {
            if(!strncmp(tm.name_ptr, "add_server", tm.name_len)) {
                if(tm.fields[1].present && tm.fields[2].present &&
                   tm.fields[3].present) {
                    char *ip;
                    int ip_len;
                    int port;
                    int key;
                    
                    ip = tm.fields[1].val_ptr;
                    ip_len = tm.fields[1].val_len;
                    port = tm.fields[2].val_val;
                    key = tm.fields[3].val_val;

                    add_server(ip, ip_len, port, key);
                }
            }
            else if(!strncmp(tm.name_ptr, "release_servers", tm.name_len)) {
                release_servers();
            }
            else {
                printk("Unrecognized function call %.*s\n", tm.name_len, 
                       tm.name_ptr);
            }
            ThriftSocket_discard(ts);
        }
    }

}

static void server_event(ThriftSocket *ts, void *data, int event_flags)
{
    Server *server = (Server *) data;
    
    if(event_flags & STATE_CHANGED) {
        if(TCP_SOCK_CLOSED(ts->bs.sock)) {
            ThriftSocket_cleanup(ts);
            // We don't kfree(ts) here, because it's part of Server

            write_lock_server_list();
            list_del(&server->server_list);
            unlock_server_list();

            kfree(server);
            return;
        }
    }
}

static void client_event(ThriftSocket *ts, void *data, int event_flags)
{
    ThriftMessage tm;
    Server *dest = NULL;
    Server *pos = NULL;
    Client *client = (Client *) data;

    BUG_ON(client == NULL);

    if(event_flags & STATE_CHANGED) {
        if(TCP_SOCK_CLOSED(ts->bs.sock)) {
            ThriftSocket_cleanup(ts);
            // We don't kfree(ts), because ts is a field in client
            kfree(client);
            return;
        }
    }
    
    if((event_flags & DATA_READY)) {
        while(ThriftSocket_next(ts, &tm) == 1) {
            dest = NULL;
            if(tm.fields[KEY_ID].present) {
                int key = tm.fields[KEY_ID].val_val;
                
                read_lock_server_list();
                list_for_each_entry(pos, &server_list, server_list) {
                    if(pos->key == key) {
                        dest = pos;
                        break;
                    }
                }
                unlock_server_list();
            }
            
            if(dest != NULL) {
                ThriftSocket_forward(ts, &dest->ts);
                ThriftSocket_discard(ts);
            }
            else {
                /* Give up and go home */
                return;
            }
        }
    }
}

static void controller_listener_event(struct socket *lsock, void *data,
                                      int event_flags)
{
    int rcode;
    ThriftSocket *ts = NULL;

    if(event_flags & DATA_READY) {
        do {
            ts = kmalloc(sizeof(*ts), GFP_KERNEL);
            check(ts != NULL, "kmalloc failed");

            rcode = ThriftSocket_accept(lsock, ts, controller_event, NULL);

            if(rcode != 0) {
                kfree(ts);
                ts = NULL;
            }

        } while(rcode == 0);
    }

    return;

error:
    if(ts) kfree(ts);
}

static void client_listener_event(struct socket *lsock, void *data,
                                  int event_flags)
{
    int rcode;
    Client *client = NULL;

    if(event_flags & DATA_READY) {
        do {
            client = kmalloc(sizeof(*client), GFP_KERNEL);
            check(client != NULL, "kmalloc failed");

            rcode = ThriftSocket_accept(lsock, &client->ts, client_event,
                                        client);
            if(rcode != 0) {
                kfree(client);
                client = NULL;
            }

        } while(rcode == 0);
    }

    return;

error:
    if(client) kfree(client);
}

static int start_listener(short port,
                          void (*event)(struct socket *, void *, int),
                          void *data,
                          struct socket **sp)
{
    struct socket *listen_sock;
    struct sockaddr_in myaddr;  
    int rcode;

    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = htons(port);
    myaddr.sin_family = AF_INET;

    rcode = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &listen_sock);
    check(rcode == 0, "sock_create failed");

    rcode = listen_sock->ops->bind(listen_sock, (struct sockaddr*) &myaddr,
                                   sizeof(myaddr));
    check(rcode == 0, "bind failed");

    rcode = listen_sock->ops->listen(listen_sock, BACKLOG_SIZE);
    check(rcode == 0, "listen failed");
  
    register_sock(listen_sock, event, data);
    *sp = listen_sock;

    return 0;

error:
    if(listen_sock != NULL)
        sock_release(listen_sock);

    return -1;
}

static int __init init_forwarder(void) {
    int rcode = 0;

    // This is probably unnecessary, but it doesn't hurt anything
    INIT_LIST_HEAD(&server_list);

    rcode = start_listener(CLIENT_LISTEN_PORT, client_listener_event, NULL,
                           &client_listener);
    check(rcode == 0, "failed to start client_listener");

    rcode = start_listener(CONTROLLER_LISTEN_PORT, controller_listener_event, 
                           NULL, &controller_listener);
    check(rcode == 0, "failed to start client_listener");

    return 0;

error:
    return -1;
}

static void __exit exit_forwarder(void) {
    release_servers();

    if(client_listener != NULL) {
        unregister_sock(client_listener);
        sock_release(client_listener);
    }

    if(controller_listener != NULL) {
        unregister_sock(controller_listener);
        sock_release(controller_listener);
    }
}



module_init(init_forwarder);
module_exit(exit_forwarder);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Alex Gartrell");
MODULE_DESCRIPTION("Module to route simple thrift requests");


