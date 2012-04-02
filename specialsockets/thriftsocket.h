#ifndef _THRIFTSOCKET_H
#define _THRIFTSOCKET_H

#include "bufferedsocket.h"

#define MAX_FIELD_ID 20

typedef enum ThriftFieldType {
    T_STOP       = 0,
    T_VOID       = 1,
    T_BOOL       = 2,
    T_BYTE       = 3,
    T_I08        = 3,
    T_I16        = 6,
    T_I32        = 8,
    T_U64        = 9,
    T_I64        = 10,
    T_DOUBLE     = 4,
    T_STRING     = 11,
    T_UTF7       = 11,
    T_STRUCT     = 12,
    T_MAP        = 13,
    T_SET        = 14,
    T_LIST       = 15,
    T_UTF8       = 16,
    T_UTF16      = 17
} ThriftFieldType;

typedef enum ThriftMessageType {
    T_CALL       = 1,
    T_REPLY      = 2,
    T_EXCEPTION  = 3,
    T_ONEWAY     = 4
} ThriftMessageType;

typedef struct ThriftField {
    int present;

    ThriftFieldType fieldType;
    
    int val_len;
    char *val_ptr;
    
    // Included for convencience.  Only works for 8, 16, and 32-bit types
    int val_val;
} ThriftField;

typedef struct ThriftMessage {
    ThriftMessageType messageType;

    // name is (necessarily) not NULL-terminated, fyi
    int name_len;
    char *name_ptr;

    int seqid;

    ThriftField fields[MAX_FIELD_ID + 1];
} ThriftMessage;

typedef struct ThriftSocket {
    BufferedSocket bs;
    void (*event)(struct ThriftSocket *, void *, int);
    void *message_start;
    int message_len;
} ThriftSocket;

int ThriftSocket_init(ThriftSocket *ts, struct socket *sock, 
                      void (*event)(ThriftSocket *, void *, int),
                      void *data);
int ThriftSocket_cleanup(ThriftSocket *ts);
int ThriftSocket_accept(struct socket *lsock, ThriftSocket *ts, 
                        void (*event)(ThriftSocket *, void *, int), 
                        void *data);

int ThriftSocket_next(ThriftSocket *ts, ThriftMessage *tm);
int ThriftSocket_discard(ThriftSocket *ts);
int ThriftSocket_write(ThriftSocket *ts, ThriftMessage *tm);
int ThriftSocket_forward(ThriftSocket *from, ThriftSocket *to);

#endif
