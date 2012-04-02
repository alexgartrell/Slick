#ifndef _KLIBEVENT_H
#define _KLIBEVENT_H

#include <net/tcp_states.h> /* TCP_CLOSE(_WAIT) */
#include <net/sock.h> /* for struct sock */

#define DATA_READY (1 << 0)
#define STATE_CHANGED (1 << 1)
#define WRITE_SPACE (1 << 2)


#define TCP_SOCK_CLOSED(sock) (((sock)->sk->sk_state == TCP_CLOSE_WAIT) || \
                               ((sock)->sk->sk_state == TCP_CLOSE))


struct socket;

typedef void (*notify_func_t)(struct socket *, void *, int);

int register_sock(struct socket *, notify_func_t, void *);
void unregister_sock(struct socket *);

#endif
