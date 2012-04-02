#ifndef _MEMCACHED_CACHE_H
#define _MEMCACHED_CACHE_H

#define CACHE_NOT_FOUND (-1)

#define KEYLEN_MAX 20 /* 160-bit keys */
#define DATALEN_MAX 256 /* arbitrary guesswork */

#define MIN(x,y) ({ typeof(x) _x = (x); typeof(y) _y = (y); _x<_y ? _x : _y; })

int cache_find(void *key, int keylen, void *data, int *datalen, int max_datalen);
int cache_insert(void *key, int keylen, void *data, int datalen);

#endif
