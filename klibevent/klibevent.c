#include "klibevent.h"

#include <linux/net.h> /* struct socket */
#include <net/sock.h> /* struct sock */
#include <net/tcp_states.h> /* TCP_CLOSE(_WAIT) */

#include <linux/slab.h> /* kmalloc, kfree */

#include <linux/workqueue.h> /* queue_work */

#define queue_cont(cont) do {                                   \
        if(!atomic_xchg(&((cont)->queued), true))               \
            queue_work(klibevent_work_queue, &((cont)->work));  \
    } while(0)


// typedef void (*notify_func_t)(struct socket *, void *, int);

/* static functions */
static void overwrite_callbacks(struct sock *sk, void *data);
static void reset_callbacks(struct sock *sk);

static void work_callback(struct work_struct *work);
static void klibevent_utility_callback(struct work_struct *work);

/* overwrite callbacks for sockets [1] */
static void over_sk_state_change(struct sock *sk);
static void over_sk_data_ready(struct sock *sk, int bytes);
static void over_sk_write_space(struct sock *sk);

/* static variables */
static int standard_callbacks_not_initialized = true;
static void (*standard_sk_state_change)(struct sock *sk) = NULL;
static void (*standard_sk_data_ready)(struct sock *sk, int bytes) = NULL;
static void (*standard_sk_write_space)(struct sock *sk) = NULL;
static struct workqueue_struct *klibevent_work_queue;
static struct work_struct klibevent_utility_work;

typedef struct sock_container sock_container_t;
struct sock_container {
    struct socket *sock;
    notify_func_t func;
    void *data;

    atomic_t queued;
    struct work_struct work;
  
    long state_change;
    long data_ready;
    long write_space;
};

int register_sock(struct socket *sock, notify_func_t func, void *data) {
    sock_container_t *cont = kmalloc(sizeof(*cont), GFP_KERNEL);
    if(cont == NULL) return -1;
    memset(cont, 0, sizeof(*cont));
    atomic_set(&cont->queued, false);
    INIT_WORK(&cont->work, work_callback);
    cont->data = data;
    cont->func = func;
    cont->sock = sock;

    // Maybe with lock?
    overwrite_callbacks(sock->sk, cont);

    cont->state_change = true;
    cont->data_ready = true;
    cont->write_space = true;
    queue_cont(cont);

    return 0;
}

void unregister_sock(struct socket *sock) {
    sock_container_t *cont;
    if(sock == NULL) return;
    cont = (sock_container_t*) sock->sk->sk_user_data;
    reset_callbacks(sock->sk);
    cont->sock = NULL;
    queue_cont(cont);
}

void overwrite_callbacks(struct sock *sk, void *data) {
    if(unlikely(standard_callbacks_not_initialized)) {
        standard_callbacks_not_initialized = false;
        klibevent_work_queue = create_singlethread_workqueue("klibevent");
        INIT_WORK(&klibevent_utility_work, klibevent_utility_callback);        
        standard_sk_state_change = sk->sk_state_change;
        standard_sk_data_ready = sk->sk_data_ready;
        standard_sk_write_space = sk->sk_write_space;
    }
    sk->sk_user_data = data;
    sk->sk_state_change = over_sk_state_change;
    sk->sk_data_ready = over_sk_data_ready;
    sk->sk_write_space = over_sk_write_space;
}

void reset_callbacks(struct sock *sk) {
    if(sk == NULL) {
        printk("sk is NULL at slicksock_reset_callbacks\n");
        return;
    }
    sk->sk_user_data = NULL;
    sk->sk_state_change = standard_sk_state_change;
    sk->sk_data_ready = standard_sk_data_ready;
    sk->sk_write_space = standard_sk_write_space;
}

static void klibevent_utility_callback(struct work_struct *work) {
    /* Increase the priority of the worker thread */
    set_user_nice(current, -19);
}

static void work_callback(struct work_struct *work) {
    sock_container_t *cont = container_of(work, sock_container_t, work);
    int event_flags = 0;

    /* This is true after we unregister the sock */
    if(unlikely(cont->sock == NULL)) {
        kfree(cont);
        return;
    }

    if(cont->state_change) {
        event_flags |= STATE_CHANGED;
        cont->state_change = false;
    }
    if(cont->data_ready) {
        event_flags |= DATA_READY;
        cont->data_ready = false;
    }
    if(cont->write_space) {
        event_flags |= WRITE_SPACE;
        cont->write_space = false;
    }
    cont->func(cont->sock, cont->data, event_flags);
  
    atomic_set(&cont->queued, false);
    if(cont->state_change || cont->data_ready || cont->write_space
       || cont->sock == NULL)
        queue_cont(cont);
}

/* modified callbacks for sockets [1] */
static void over_sk_state_change(struct sock *sk) {
    sock_container_t *cont = (sock_container_t*) sk->sk_user_data;
    if(likely(cont != NULL)) {
        cont->state_change = true;
        queue_cont(cont);
    }
    standard_sk_state_change(sk);
}

static void over_sk_data_ready(struct sock *sk, int bytes) {
    sock_container_t *cont = (sock_container_t*) sk->sk_user_data;
    if(likely(cont != NULL)) {
        cont->data_ready = true;
        queue_cont(cont);
    }
    standard_sk_data_ready(sk, bytes);
}

static void over_sk_write_space(struct sock *sk) {
    sock_container_t *cont = (sock_container_t*) sk->sk_user_data;
    if(likely(cont != NULL)) {
        cont->write_space = true;
        queue_cont(cont);
    }
    standard_sk_write_space(sk);
}

/* Notes:
 * [1] sk_* functions are callbacks issued by various protocol-specific
 *       functions that are triggered for specific events.
 *     - sk_state_change - called when the TCP state has changed
 *     - sk_data_ready - called when there is extra content in the recieve queue
 *     - sk_write_space - called when there is extra space in the send queue
 *     - sk_error_report - called upon error.  sk_err is set appropriately 
 *     - sk_backlog_rcv - called when packets need to be added to the read
 *                        buffer probably should not be overwritten entirely
 *     - sk_destruct - called upon destruction of the sock NOTE: invoked as
 *                       by sock_release, so overwriting it is unnecessary 
 *     The best way to find specific usage for the functions is to
 *       $ cd /path/to/linux/net/ipv4
 *       $ find . | xargs grep 'sk_data_ready(' # or sk_write_space, etc.
 */
