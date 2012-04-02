#ifndef __CIRCBUFFER_H
#define __CIRCBUFFER_H

#include <linux/string.h>

// Works because 1024 is a power of 2
#define BUFFER_LEN (1 << 15)
#define MOD_BUFFER_LEN(x) ((x) & (BUFFER_LEN - 1))

typedef struct CircBuffer {
    char buffer[BUFFER_LEN];
    // Including this in each CircBuffer is kind of a waste, but it's safe
    // and easy
    char scratch[BUFFER_LEN];
    int start;
    int len;
} CircBuffer;

static inline void CircBuffer_init(CircBuffer *cb) {
    cb->start = 0;
    cb->len = 0;
}

static inline void CircBuffer_reset(CircBuffer *cb) {
    if(cb->len == 0) {
        cb->start = 0;
    }
    else if(cb->start + cb->len <= BUFFER_LEN) {
        memmove(cb->buffer, cb->buffer + cb->start, cb->len);
        cb->start = 0;
    }
    else {
        int chunk1_len = BUFFER_LEN - cb->start;
        int chunk2_len = cb->start + cb->len - BUFFER_LEN;

        memcpy(cb->scratch, cb->buffer, chunk2_len);
        memmove(cb->buffer, cb->buffer + cb->start, chunk1_len);
        memcpy(cb->buffer + chunk1_len, cb->scratch, chunk2_len);
        cb->start = 0;
    }
}

static inline int CircBuffer_read_peek(CircBuffer *cb, char **p) {
    if(cb->len == 0) {
        *p = NULL;
        return 0;
    }

    *p = cb->buffer + cb->start;

    if(cb->len + cb->start > BUFFER_LEN)
        return BUFFER_LEN - cb->start;

    return cb->len;
        
}

static inline int CircBuffer_read_commit(CircBuffer *cb, int amt) {
    if(amt > cb->len) return -1;

    cb->len -= amt;
    cb->start = MOD_BUFFER_LEN(cb->start + amt);

    return 0;
}

static inline int CircBuffer_read(CircBuffer *cb, char *buffer, int max) {
    char *p;
    int amt;
    int total = 0;
    while((amt = CircBuffer_read_peek(cb, &p)) > 0 && total < max) {
        if(total + amt > max)
            amt = max - total;
        memcpy(buffer + total, p, amt);
        total += amt;
        CircBuffer_read_commit(cb, amt);
    }

    return total;
}

static inline int CircBuffer_write_peek(CircBuffer *cb, char **p) {
    if(cb->len + cb->start > BUFFER_LEN) {
        *p = cb->buffer + cb->start + cb->len - BUFFER_LEN;
        return BUFFER_LEN - cb->len;
    }

    *p = cb->buffer + cb->start + cb->len;
    return BUFFER_LEN - (cb->start + cb->len);
}

static inline int CircBuffer_write_commit(CircBuffer *cb, int amt) {
    if(cb->len + amt > BUFFER_LEN) return -1;
    
    cb->len += amt;
    return 0;
}

static inline int CircBuffer_write(CircBuffer *cb, char *p, int len) {
    int total = 0;
    int off = MOD_BUFFER_LEN(cb->start + cb->len);

    while(cb->len < BUFFER_LEN && total < len) {
        int amt = (off < cb->start) ? (cb->start - off) : (BUFFER_LEN - off);
        if(amt > len - total) amt = len - total;

        memcpy(cb->buffer + off, p + total, amt);
        
        total += amt;
        cb->len += amt;
        off = MOD_BUFFER_LEN(off + amt);
    }

    return total;
}

#define CircBuffer_len(cb) ((cb)->len)
#define CircBuffer_remaining(cb) (BUFFER_LEN - (cb)->len)
#define CircBuffer_full(cb) ((cb)->len == BUFFER_LEN)

#endif
