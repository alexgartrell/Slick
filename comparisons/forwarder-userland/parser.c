#include <stdio.h>
#include <string.h>
#include <parser.h>

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


int read_thrift(char *ptr, int len, ThriftMessage *tm)
{
    int off;
    int x;
    int fieldId;
    char fieldType;

    memset(tm, 0, sizeof(*tm));

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
            printf("Ignoring field with out of range id, id = %d, max = %d",
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

    return off;
too_short:
    return 0;
}
