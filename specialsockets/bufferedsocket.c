#include "bufferedsocket.h"

#include "../klibevent/klibevent.h"

static void handle_event(struct socket *, void *, int);
static void fill_read_buffer(BufferedSocket *bs);
static void empty_write_buffer(BufferedSocket *bs);
static int bs_send_buff(BufferedSocket *bs, char *buff, int len);
static int bs_recv_buff(BufferedSocket *bs, char *buff, int len);

int BufferedSocket_init(BufferedSocket *bs, struct socket *sock,
                        void (*event)(BufferedSocket *bs, void *, int),
                        void *data)
{
    if(!bs || !sock) return -1;

    bs->sock = sock;
    CircBuffer_init(&bs->rdbuffer);
    CircBuffer_init(&bs->wrbuffer);

    bs->event = event;
    bs->data = data;

    return register_sock(sock, handle_event, bs);
}

int BufferedSocket_cleanup(BufferedSocket *bs) {
    if(!bs) return -1;

    unregister_sock(bs->sock);

    sock_release(bs->sock);

    return 0;
}

void BufferedSocket_reset_buffer(BufferedSocket *bs) {
    CircBuffer_reset(&bs->rdbuffer);
}

int BufferedSocket_peek(BufferedSocket *bs, char **buffer) {
    if(!bs || !buffer) return -1;

    fill_read_buffer(bs);

    return CircBuffer_read_peek(&bs->rdbuffer, buffer);
}

int BufferedSocket_discard(BufferedSocket *bs, int len) {
    if(!bs) return -1;

    return CircBuffer_read_commit(&bs->rdbuffer, len);
}

int BufferedSocket_read(BufferedSocket *bs, char *buff, int max_len) {
    if(!bs || !buff || max_len <= 0) return -1;

    fill_read_buffer(bs);

    return CircBuffer_read(&bs->rdbuffer, buff, max_len);
}

int BufferedSocket_write_all(BufferedSocket *bs, char *buff, int len) {
    int sent;

    // We should *probably* be locking here

    // We're being cautious here, it's possible that we could send it all off
    // without hitting the write buffer, but better safe than sorry
    if(CircBuffer_remaining(&bs->wrbuffer) < len)
        return 0;

    sent = BufferedSocket_write(bs, buff, len);
    
    BUG_ON(sent != len);

    return sent;
}

int BufferedSocket_write(BufferedSocket *bs, char *buff, int len) {
    int amt, sent = 0;

    if(!bs || !buff || len <= 0) return -1;
    
    // Not sure if this is a good idea or not
    if(CircBuffer_len(&bs->wrbuffer) > 0) empty_write_buffer(bs);

    // Optimization, go to the socket first if we can.
    if(CircBuffer_len(&bs->wrbuffer) == 0) {
        while(sent < len) {
            amt = bs_send_buff(bs, buff + sent, len - sent);
            
            // Out of Space
            if(amt <= 0) break;
            
            sent += amt;
        }
    }

    // And now we're adding the remainder (which may or may not exist) to the
    // CircBuffer.  It'll get wiped out when there's a WRITE_SPACE event.
    if(sent < len)
        sent += CircBuffer_write(&bs->wrbuffer, buff + sent, len - sent);

    // Try to empty our buffer out into the ether.  This is necessary, because
    // otherwise, it'll just get stale here.  We need to push for the
    // WRITE_SPACE event (I think)
    empty_write_buffer(bs);

    return sent;
}

static void handle_event(struct socket *sock, void *data, int event_flags) {
    BufferedSocket *bs = (BufferedSocket *) data;
    
    if((event_flags & WRITE_SPACE))
        empty_write_buffer(bs);

    bs->event(bs, bs->data, event_flags);
    // Can't do anything with bs after this, they might have freed it
}

static int bs_send_buff(BufferedSocket *bs, char *buff, int len) {
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

    iov.iov_base = buff;
    iov.iov_len = len;

    return sock_sendmsg(bs->sock, &msg, len);
}

static int bs_recv_buff(BufferedSocket *bs, char *buff, int len) {
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

    iov.iov_base = buff;
    iov.iov_len = len;

    return sock_recvmsg(bs->sock, &msg, len, MSG_DONTWAIT);
}

static void fill_read_buffer(BufferedSocket *bs) {
    char *p;
    int max, amt;

    while((max = CircBuffer_write_peek(&bs->rdbuffer, &p)) > 0) {
        amt = bs_recv_buff(bs, p, max);
        if(amt <= 0) return;

        CircBuffer_write_commit(&bs->rdbuffer, amt);
    }
}

static void empty_write_buffer(BufferedSocket *bs) {
    char *p;
    int amt, sent;

    while((amt = CircBuffer_read_peek(&bs->wrbuffer, &p)) > 0) {
        sent = bs_send_buff(bs, p, amt);
        if(sent <= 0) return;
        CircBuffer_read_commit(&bs->wrbuffer, sent);
    }
}

