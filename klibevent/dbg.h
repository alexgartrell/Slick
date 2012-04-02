#ifndef _DBG_H
#define _DBG_H

#define log(M, ...) {                           \
        printk(M "\n", ##__VA_ARGS__);          \
    }

#define check(A, M, ...) {                      \
        if(!(A)) {                              \
            log(M, ##__VA_ARGS__);              \
            goto error;                         \
        }                                       \
    }

#endif
