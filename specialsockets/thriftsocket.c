#include <klibevent.h>
#include <bufferedsocket.h>
#include <thriftsocket.h>

#include <dbg.h>

#include <linux/kernel.h>

static void ts_handle_event(BufferedSocket *bs, void *data, int event_flags);

int ThriftSocket_init(ThriftSocket *ts, struct socket *sock, 
                      void (*event)(ThriftSocket *, void *, int),
                      void *data)
{
    BUG_ON(ts == NULL);

    ts->event = event;
    ts->message_len = 0;

    return BufferedSocket_init(&ts->bs, sock, ts_handle_event, data);
}

int ThriftSocket_cleanup(ThriftSocket *ts)
{
    BUG_ON(ts == NULL);

    return BufferedSocket_cleanup(&ts->bs);
}

int ThriftSocket_accept(struct socket *lsock, ThriftSocket *ts, 
                        void (*event)(ThriftSocket *, void *, int), 
                        void *data)
{
    struct socket *csock = NULL;
    int rcode;

    BUG_ON(lsock == NULL || ts == NULL || event == NULL);

    rcode = sock_create_lite(PF_INET, SOCK_STREAM, IPPROTO_TCP, &csock);
    check(rcode == 0, "sock_create_lite failed");

    csock->ops = lsock->ops;
    rcode = lsock->ops->accept(lsock, csock, O_NONBLOCK);

    // We fail without a log message if accept fails, no one to accept
    if(rcode) goto error;
    
    rcode = ThriftSocket_init(ts, csock, event, data);
    check(rcode == 0, "ThriftSocket_init failed");

    return 0;
    
error:
    if(csock != NULL) sock_release(csock);
    return -1;
}

#define CHECK_LEN(off, len, needed) {                   \
        if((len) - (off) < (needed)) goto too_short;    \
    }

#define READ_I32(ptr, off, len, i32) {          \
        CHECK_LEN(off, len, 4);                 \
        memcpy(&(i32), ptr + off, 4);           \
        i32 = ntohl(i32);                       \
        off += 4;                               \
    }

#define READ_I16(ptr, off, len, i16) {          \
        CHECK_LEN(off, len, 2);                 \
        memcpy(&(i16), ptr + off, 4);           \
        i16 = ntohs(i16);                       \
        off += 2;                               \
    }

#define READ_I8(ptr, off, len, i8) {            \
        CHECK_LEN(off, len, 1);                 \
        i8 = ptr[off];                          \
        off += 1;                               \
    }

#define READ_STRING(ptr, off, len, str_len, str_ptr) {      \
        READ_I32(ptr, off, len, str_len);                   \
        CHECK_LEN(off, len, str_len);                       \
        str_ptr = ptr + off;                                \
        off += str_len;                                     \
    }

#define WRITE_I32(ptr, off, len, i32) {         \
        int x = htonl(i32);                     \
        CHECK_LEN(off, len, 4);                 \
        memcpy(ptr + off, &x, 4);               \
        off += 4;                               \
    }

#define WRITE_I16(ptr, off, len, i16) {         \
        short x = htons(i16);                   \
        CHECK_LEN(off, len, 2);                 \
        memcpy(ptr + off, &x, 2);               \
        off += 2;                               \
    }

#define WRITE_I8(ptr, off, len, i8) {           \
        char x = i8;                            \
        CHECK_LEN(off, len, 1);                 \
        memcpy(ptr + off, &x, 1);               \
        off += 1;                               \
    }

#define Write_STRING(ptr, off, len, str_len, str_ptr) {     \
        WRITE_I32(ptr, off, len, str_len);                  \
        CHECK_LEN(off, len, str_len);                       \
        memcpy(ptr + off, str_ptr, str_len);                \
        off += str_len;                                     \
    }

int ThriftSocket_next(ThriftSocket *ts, ThriftMessage *tm)
{
    char *ptr;
    int off, len;
    int x;
    int fieldId;
    char fieldType;

    BUG_ON(ts == NULL || tm == NULL);

    memset(tm, 0, sizeof(*tm));

    // This is lazy, and should probably be changed
    BufferedSocket_reset_buffer(&ts->bs);

    len = BufferedSocket_peek(&ts->bs, &ptr);
    off = 0;

    READ_I32(ptr, off, len, x);
    // ASSERT version stuff
    tm->messageType = x & 0xff;

    READ_STRING(ptr, off, len, tm->name_len, tm->name_ptr);
    
    READ_I32(ptr, off, len, tm->seqid);

    while(1) {
        READ_I8(ptr, off, len, fieldType);
        if(fieldType == T_STOP)
            break;

        READ_I16(ptr, off, len, fieldId);
        if(fieldId > MAX_FIELD_ID || fieldId < 0) {
            printk("Ignoring field with out of range id, id = %d, max = %d",
                  MAX_FIELD_ID, fieldId);
            fieldId = -1;
        }

        if(fieldId >= 0) {
            tm->fields[fieldId].present = 1;
            tm->fields[fieldId].fieldType = fieldType;
        }
        
        switch(fieldType) {
        case T_STOP:
            break;
        case T_VOID:
            continue;
        case T_BOOL:
        // case T_BYTE: would be a dupe
        case T_I08:
            CHECK_LEN(off, len, 1);
            if(fieldId >= 0) {
                tm->fields[fieldId].val_len = 1;
                tm->fields[fieldId].val_ptr = ptr + off;
                tm->fields[fieldId].val_val = (int) ptr[off];
            }
            off += 1;
            break;
        case T_I16:
            CHECK_LEN(off, len, 2);
            if(fieldId >= 0) {
                short s;
                tm->fields[fieldId].val_len = 2;
                tm->fields[fieldId].val_ptr = ptr + off;
                memcpy(&s, ptr + off, 2);
                tm->fields[fieldId].val_val = (int) ntohs(s);
            }
            off += 2;
            break;
        case T_I32:
            CHECK_LEN(off, len, 4);
            if(fieldId >= 0) {
                int i;
                tm->fields[fieldId].val_len = 4;
                tm->fields[fieldId].val_ptr = ptr + off;
                memcpy(&i, ptr + off, 4);
                tm->fields[fieldId].val_val = ntohl(i);
            }
            off += 4;
            break;
        case T_U64:
        case T_I64:
        case T_DOUBLE:
            CHECK_LEN(off, len, 8);
            if(fieldId >= 0) {
                tm->fields[fieldId].val_len = 8;
                tm->fields[fieldId].val_ptr = ptr + off;
            }
            off += 8;
            break;
            
        // Treat all of these as just length-delimited
        case T_STRING:
        // case T_UTF7: duplicate
        case T_STRUCT:
        case T_MAP:
        case T_SET:
        case T_LIST:
        case T_UTF8:
        case T_UTF16:
            if(fieldId >= 0) {
                READ_STRING(ptr, off, len, tm->fields[fieldId].val_len,
                            tm->fields[fieldId].val_ptr);
            }
            else
            {
                char *p_ign;
                int l_ign;
                READ_STRING(ptr, off, len, l_ign, p_ign);
            }
            break;
        }
    }

    ts->message_start = ptr;
    ts->message_len = off;

    return 1;
too_short:
    return 0;
}

int ThriftSocket_discard(ThriftSocket *ts)
{
    BUG_ON(ts == NULL || ts->message_len <= 0);

    BufferedSocket_discard(&ts->bs, ts->message_len);
    ts->message_len = 0;
    return -1;
}

int ThriftSocket_forward(ThriftSocket *from, ThriftSocket *to) {
    BUG_ON(from == NULL || to == NULL);
    
    return BufferedSocket_write_all(&to->bs, from->message_start,
                                    from->message_len) != from->message_len;
}

int ThriftSocket_write(ThriftSocket *ts, ThriftMessage *tm)
{
    BUG_ON(ts == NULL || tm == NULL);
    
    // This code is going to suck to write, I think, if I want to guarantee
    // atomicity -- and I should

    return 0;
}

static void ts_handle_event(BufferedSocket *bs, void *data, int event_flags) {
    ThriftSocket *ts;
    BUG_ON(bs == NULL);

    ts = container_of(bs, ThriftSocket, bs);

    ts->event(ts, data, event_flags);
}
