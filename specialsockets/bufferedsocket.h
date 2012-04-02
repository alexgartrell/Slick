#ifndef _BUFFEREDSOCKET_H
#define _BUFFEREDSOCKET_H

#include "circbuffer.h"

typedef struct BufferedSocket {
    struct socket *sock;
    CircBuffer rdbuffer;
    CircBuffer wrbuffer;
    void (*event)(struct BufferedSocket *, void *, int);
    void *data;
} BufferedSocket;

int BufferedSocket_init(BufferedSocket *bs, struct socket *sock,
                        void (*event)(BufferedSocket *bs, void *, int),
                        void *data);
int BufferedSocket_cleanup(BufferedSocket *bs);

void BufferedSocket_reset_buffer(BufferedSocket *bs);
int BufferedSocket_peek(BufferedSocket *bs, char **buffer);
int BufferedSocket_discard(BufferedSocket *bs, int len);
int BufferedSocket_read(BufferedSocket *bs, char *buff, int max_len);
int BufferedSocket_write(BufferedSocket *bs, char *buff, int max_len);
int BufferedSocket_write_all(BufferedSocket *bs, char *buff, int len);

#endif
