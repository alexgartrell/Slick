#include "cache.h"

#include <linux/bug.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>

struct cache_entry {
	char key[KEYLEN_MAX];
	int keylen;
	char data[DATALEN_MAX];
	int datalen;
	struct rb_node rb;
	/* TODO: for cache eviction, make a second rbtree entry here */
};

static struct rb_root memcache_root = { .rb_node = NULL };

static DEFINE_RWLOCK(memcache_lock);

int cache_find(void *key, int keylen, void *data, int *datalen, int max_datalen)
{
	/* for iterating through the cache */
	struct rb_node *p = memcache_root.rb_node;
	struct cache_entry *entry;
	/* key comparison result */
	int diff;

	read_lock(&memcache_lock);

	/* this implementation is taken straight from include/linux/rbtree.h */
	while (p) {
		entry = rb_entry(p, struct cache_entry, rb);
		diff = entry->keylen - keylen;
		if (!diff)
			diff = memcmp(key, entry->key, keylen);

		if (diff < 0) {
			p = p->rb_left;
		} else if (diff > 0) {
			p = p->rb_right;
		} else {
			/* item found */
			*datalen = MIN(max_datalen, entry->datalen);
			memcpy(data, entry->data, MIN(max_datalen, entry->datalen));
			read_unlock(&memcache_lock);
			return 0;
			/* TODO: if this is multithreaded, need locking here */
		}
	}

	read_unlock(&memcache_lock);
	return CACHE_NOT_FOUND;
}

/* returns 0 on successful insertion, <0 on fail */
int cache_insert(void *key, int keylen, void *data, int datalen)
{
	/* for iterating through the cache */
	struct rb_node **p = &memcache_root.rb_node;
	struct rb_node *parent = NULL;
	struct cache_entry *entry;
	struct cache_entry *new = kmalloc(sizeof(struct cache_entry), GFP_KERNEL);
	/* key comparison result */
	int diff;

	if (new == NULL) {
		return -ENOMEM;
	}

	/* truncate */
	keylen = MIN(KEYLEN_MAX, keylen);
	datalen = MIN(DATALEN_MAX, datalen);

	new->keylen = keylen;
	memcpy(new->key, key, keylen);
	new->datalen = datalen;
	memcpy(new->data, data, datalen);

	write_lock(&memcache_lock);

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct cache_entry, rb);
		diff = entry->keylen - keylen;
		if (!diff)
			diff = memcmp(key, entry->key, keylen);

		if (diff < 0) {
			p = &(*p)->rb_left;
		} else if (diff > 0) {
			p = &(*p)->rb_right;
		} else {
			/* key already in cache. TODO: evict */
			write_unlock(&memcache_lock);
			kfree(new);
			return -1;
		}
	}

	rb_link_node(&new->rb, parent, p);
	rb_insert_color(&new->rb, &memcache_root);

	write_unlock(&memcache_lock);
	return 0;
}
