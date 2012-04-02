#ifndef _PARSER_H
#define _PARSER_H

#define MAX_FIELD_ID 100

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


int read_thrift(char *ptr, int len, ThriftMessage *tm);

#endif
