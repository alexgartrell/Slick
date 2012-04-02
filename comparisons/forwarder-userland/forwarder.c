#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <fcntl.h>

#include <signal.h>
#include <errno.h>

#include <event.h>

#include <circbuffer.h>
#include <parser.h>

#define check(A, M, ...) {                              \
        if(!(A)) {                                      \
            fprintf(stderr, "%s:%d: "M "\n",              \
                    __FILE__, __LINE__, ##__VA_ARGS__); \
            goto error;                                 \
        }                                               \
    }

#define CONTROL_PORT 54321
#define DATA_PORT 12345

typedef struct Client {
    CircBuffer buff;
    int fd;
} Client;

typedef struct Controller {
    CircBuffer buff;
    int fd;
} Controller;

typedef struct Server {
    int key;
    int fd;
    struct bufferevent *bev;
    struct Server *next;
    struct Server *prev;
} Server;

/* socket helper functions */
static int create_listener(unsigned short);
static int create_connection(char *, int, int);
static int set_nonblocking(int);

/* libevent callbacks */
static void client_accept(int, short, void *);
static void client_read(struct bufferevent *, void *);
static void client_error(struct bufferevent *, short, void *);
static void controller_accept(int, short, void *);
static void controller_read(struct bufferevent *, void *);
static void controller_error(struct bufferevent *, short, void *);
static void server_error(struct bufferevent *, short, void *);

/* RPC functions */
static void add_server(char *, int, int, int);
static void release_servers(void);

/* other? */
static void top_off_circbuffer(CircBuffer *, struct bufferevent *);
static Server *lookup_server(int);


static Server *server_list = NULL;

int main(int argc, char *argv[])
{
    int controller_listener = -1;
    int client_listener = -1;
    int rcode;
    struct event_base *event_base;

    struct event ev_client_accept;
    struct event ev_controller_accept;

    signal(SIGPIPE, SIG_IGN);

    event_base = event_init();
    check(event_base != NULL, "event_base_new() failed");

    controller_listener = create_listener(CONTROL_PORT);
    check(controller_listener >= 0, "create_listener(...) failed");
    rcode = set_nonblocking(controller_listener);
    check(rcode == 0, "set_nonblocking(...) failed");
    event_set(&ev_controller_accept, controller_listener, EV_READ|EV_PERSIST,
              controller_accept, NULL);
    event_add(&ev_controller_accept, NULL);

    client_listener = create_listener(DATA_PORT);
    check(client_listener >= 0, "create_listener(...) failed");
    rcode = set_nonblocking(client_listener);
    check(rcode == 0, "set_nonblocking(...) failed");
    event_set(&ev_client_accept, client_listener, EV_READ|EV_PERSIST,
              client_accept, NULL);
    event_add(&ev_client_accept, NULL);

    event_dispatch();

    // event_dispatch should never return
    check(0, "event_dispatch() failed");


    close(client_listener);
    close(controller_listener);
    return 0;

error:
    if(controller_listener >= 0) close(controller_listener);
    if(client_listener >= 0) close(client_listener);
    return 1;
}

static int set_nonblocking(int fd)
{
    int flags = -1;
    int rcode;

    flags = fcntl(fd, F_GETFL);
    check(flags >= 0, "fcntl(...) failed");

    flags |= O_NONBLOCK;
    rcode = fcntl(fd, F_SETFL, flags);
    check(rcode == 0, "fcntl(...) failed");

    return 0;
error:
    return -1;
}

static int create_listener(unsigned short port)
{
    int fd = -1;
    int rcode = -1;
    int v = 1;
    struct sockaddr_in addr;
    check(port > 0, "Invalid port number: %u", port);

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    check(fd >= 0, "socket(...) failed");
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    v = 1;
    rcode = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
    check(rcode == 0, "setsockopt(...) failed");

    rcode = bind(fd, (struct sockaddr *) &addr, sizeof(addr));
    check(rcode == 0, "bind(...) on port %d failed", port);

    rcode = listen(fd, 10);
    check(rcode == 0, "listen(...) failed");
    
    return fd;
error:
    perror("perror");

    if(fd >= 0) close(fd);
    return -1;
}

static int create_connection(char *ip, int ip_len, int port)
{
    int fd = -1;
    char my_ip[128];
    struct sockaddr_in addr;
    int rcode = -1;

    check(sizeof(my_ip) - 1 >= ip_len, "unreasonably long ip_len %d", ip_len);
    memcpy(my_ip, ip, ip_len);
    my_ip[ip_len] = '\0';

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    rcode = inet_pton(AF_INET, my_ip, &addr.sin_addr);
    check(rcode == 1, "inet_pton(...) failed");
    addr.sin_port = htons(port);

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    check(fd >= 0, "socket(...) failed");

    rcode = connect(fd, (struct sockaddr *) &addr, sizeof(addr));
    check(rcode == 0, "connect(...) failed");

    return fd;

error:
    perror("perror");
    if(fd >= 0) close(fd);
    return -1;
}

static void client_accept(int listener, short ev, void *arg)
{
    int fd = -1;
    struct bufferevent *bev = NULL;
    int rcode = -1;
    Client *client = NULL;

    while((fd = accept(listener, NULL, NULL)) >= 0) {
        client = malloc(sizeof(*client));
        check(client != NULL, "malloc failed");
        CircBuffer_init(&client->buff);
        client->fd = fd;

        bev = bufferevent_new(fd, client_read, NULL, client_error, client);
        check(bev != NULL, "bufferevent_new(...) failed");

        rcode = bufferevent_enable(bev, EV_READ);
        check(rcode == 0, "bufferevent_enable(...) failed");
        bev = NULL;
        client = NULL;
    }

    return;
error:
    if(bev != NULL) bufferevent_free(bev);
    if(fd >= 0) close(fd);
    if(client != NULL) free(client);
}

static void client_read(struct bufferevent *bev, void *arg)
{
    Client *client = (Client *) arg;
    char *p;
    int amt;
    int len;
    ThriftMessage tm;
    Server *server;

    top_off_circbuffer(&client->buff, bev);
    while((len = CircBuffer_read_peek(&client->buff, &p)) > 0) {
        amt = read_thrift(p, len, &tm);
        if(amt <= 0) break;

        if(tm.fields[1].present) {
            server = lookup_server(tm.fields[1].val_val);
            if(server != NULL)
                bufferevent_write(server->bev, p, amt);
        }

        CircBuffer_read_commit(&client->buff, amt);
        top_off_circbuffer(&client->buff, bev);
    }
}

static void client_error(struct bufferevent *bev, short error, void *arg)
{
    Client *client = (Client *) arg;

    bufferevent_free(bev);
    close(client->fd);
    free(client);
}

static void server_error(struct bufferevent *bev, short error, void *arg)
{
    Server *server = (Server *)arg;

    if(error == (EVBUFFER_ERROR | EVBUFFER_WRITE)) return;

    printf("server_error %d\n", error);
    /* Vanilla list delete */
    if(server->prev != NULL)
        server->prev->next = server->next;
    else
        server_list = server->next;

    if(server->next != NULL)
        server->next->prev = server->prev;

    bufferevent_free(bev);
    close(server->fd);
    free(server);
}


static void controller_accept(int listener, short ev, void *arg)
{
    int fd = -1;
    struct bufferevent *bev = NULL;
    int rcode = -1;
    Controller *controller = NULL;

    while((fd = accept(listener, NULL, NULL)) >= 0) {
        controller = malloc(sizeof(*controller));
        check(controller != NULL, "malloc failed");
        CircBuffer_init(&controller->buff);
        controller->fd = fd;

        bev = bufferevent_new(fd, controller_read, NULL, controller_error,
                              controller);
        check(bev != NULL, "bufferevent_new(...) failed");

        rcode = bufferevent_enable(bev, EV_READ);
        check(rcode == 0, "bufferevent_enable(...) failed");

        bev = NULL;
        controller = NULL;
    }
error:
    if(bev != NULL) bufferevent_free(bev);
    if(fd >= 0) close(fd);
    if(controller != NULL) free(controller);
}

static void controller_read(struct bufferevent *bev, void *arg)
{
    Controller *controller = (Controller *) arg;
    char *p;
    int amt;
    int len;
    int fd;
    ThriftMessage tm;
    Server *server;

    top_off_circbuffer(&controller->buff, bev);
    while((len = CircBuffer_read_peek(&controller->buff, &p)) > 0) {
        amt = read_thrift(p, len, &tm);
        if(amt <= 0) break;

        if(!strncmp(tm.name_ptr, "add_server", tm.name_len)) {
            if(tm.fields[1].present 
               && tm.fields[2].present 
               && tm.fields[3].present) {
                add_server(tm.fields[1].val_ptr, tm.fields[1].val_len,
                           tm.fields[2].val_val, tm.fields[3].val_val);
            }
            else {
                printf("invalid message, oh well\n");
            }
            
        }
        else if(!strncmp(tm.name_ptr, "release_servers", tm.name_len)) {
            release_servers();
        }
        else {
            printf("unrecognized function %.*s\n", tm.name_len, tm.name_ptr);
        }

        CircBuffer_read_commit(&controller->buff, amt);
        top_off_circbuffer(&controller->buff, bev);
    }
}

static void controller_error(struct bufferevent *bev, short error, void *arg)
{
    Controller *controller = (Controller *) arg;
    bufferevent_free(bev);
    close(controller->fd);
    free(controller);
}

static void release_servers()
{
    Server *s, *t;
    printf("release_servers()\n");

    s = server_list;
    while(s != NULL) {
        t = s->next;
        bufferevent_free(s->bev);
        close(s->fd);
        free(s);
        s = t;
    }
    server_list = NULL;
}

static void add_server(char *ip, int ip_len, int port, int key)
{
    int fd = -1;
    int rcode = -1;
    struct bufferevent *bev = NULL;
    Server *server = NULL;
    printf("add_server(\"%.*s\", %d, %d)\n", ip_len, ip, port, key);

    fd = create_connection(ip, ip_len, port);

    check(fd >= 0, "create_connection(...) failed");
    set_nonblocking(fd);

    server = malloc(sizeof(*server));
    check(server != NULL, "malloc(...) failed");

    bev = bufferevent_new(fd, NULL, NULL, server_error, 
                          server);
    check(bev != NULL, "bufferevent_new(...) failed");

    server->fd = fd;
    server->bev = bev;
    server->key = key;

    server->prev = NULL;

    if(server_list != NULL) {
        server_list->prev = server;
        server->next = server_list;
    }
    else
        server->next = NULL;

    server_list = server;

    rcode = bufferevent_enable(bev, 0);
    check(rcode == 0, "bufferevent_enable(...) failed");
    return;
error:
    if(bev != NULL) bufferevent_free(bev);
    if(fd >= 0) close(fd);
    if(server != NULL) free(server);
}

static Server *lookup_server(int key)
{
    Server *s;
    for(s = server_list; s != NULL; s = s->next) {
        if(s->key == key)
            return s;
    }
    return NULL;
}

static void top_off_circbuffer(CircBuffer *cb, struct bufferevent *be)
{
    int len;
    int amt;
    char *p;
    CircBuffer_reset(cb);

    while((len = CircBuffer_write_peek(cb, &p)) > 0) {
        amt = bufferevent_read(be, p, len);
        if(amt == 0) return;
        CircBuffer_write_commit(cb, amt);
    }

    return;
}

