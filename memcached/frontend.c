#include "../klibevent/klibevent.h"
#include "../specialsockets/bufferedsocket.h"
#include "cache.h"

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/socket.h>
#include <net/sock.h>

// #include <linux/slab.h>

#define PORT 19620
#define BACKLOG_SIZE 10
#define RESPONSE_MAXLEN 512

#define MSG_PREFIX "[memcached] "

#define debug_printk(...) do { } while (0)

/*
 * Communication protocol:
 * msg       ::= "<readtype> <key>\r\n" | "<writetype> <key> <data>\r\n"
 * readtype  ::= 'r' | 'q'
 * key       ::= [a-zA-Z0-9]*
 * writetype ::= 'w' | 'i'
 * data      ::= [a-zA-Z0-9]*
 */

struct socket *listen_sock;

/**
 * response shall have length RESPONSE_MAXLEN
 * returns 0 on success, <0 on faliure
 * the length of the response is shoved in rlen no matter what
 */
#define ADD_STR(...) do { \
	*rlen += snprintf(response+*rlen, BUFFER_LEN-*rlen, __VA_ARGS__); \
	} while (0)
#define MSG_CHECK(c, ...) do { \
	if (!(c)) { ADD_STR(__VA_ARGS__); return -1; } } while (0)
static int process_request(char *s, int len, char *response, int *rlen)
{
	char *key;
	int keylen;
	char *data;
	int datalen;
	char *endp;
	int ret;

	*rlen = 0;
	switch (s[0]) {
		/* read/query action */
		case 'r':
		case 'q':
			debug_printk(MSG_PREFIX "read len %d\n", len);
			MSG_CHECK(s[1] == ' ', "invalid format\r\n");
			/* find key */
			key = &s[2];
			endp = strchr(key, '\r');
			MSG_CHECK(endp != NULL, "invalid key\r\n");
			keylen = endp - key;
			/* perform the requested operation */
			ret = cache_find(key, keylen, response+*rlen, &datalen,
					 BUFFER_LEN-*rlen);
			MSG_CHECK(ret == 0, "fail\r\n");
			*rlen += datalen;
			ADD_STR("\r\nsuccess\r\n");
			break;
		/* write/insert action */
		case 'w':
		case 'i':
			debug_printk(MSG_PREFIX "write len %d\n", len);
			MSG_CHECK(s[1] == ' ', "invalid format\r\n");
			/* find key */
			key = &s[2];
			endp = strchr(key, ' ');
			MSG_CHECK(endp != NULL, "invalid key\r\n");
			keylen = endp - key;
			/* find data */
			data = &key[keylen + 1];
			endp = strchr(data, '\r');
			MSG_CHECK(endp != NULL, "invalid data\r\n");
			datalen = endp - data;
			/* perform the requested operation */
			ret = cache_insert(key, keylen, data, datalen);
			MSG_CHECK(ret == 0, "fail\r\n");
			ADD_STR("success\r\n");
			debug_printk(MSG_PREFIX "key/data len %d/%d added\r\n",
			             keylen, datalen);
			break;
		/* invalid cmd */
		default:
			ADD_STR("bad cmd %x len %d\r\n", s[0], len);
			return -1;
	}
	/* success */
	return 0;
}
#undef MSG_CHECK
#undef ADD_STR

static void client_event(BufferedSocket *bs, void *data, int event_flags)
{
	debug_printk(MSG_PREFIX "Event on client sock %ld = 0x%X\n",
	             (long) data, event_flags);

	if (event_flags & STATE_CHANGED) {
		if (TCP_SOCK_CLOSED(bs->sock)) {
			BufferedSocket_cleanup(bs);
			kfree(bs);
			return;
		}
	}

	if ((event_flags & DATA_READY)) {
		int len;
		char *p;
		char response[RESPONSE_MAXLEN];
		BUILD_BUG_ON(RESPONSE_MAXLEN < DATALEN_MAX);
		BufferedSocket_reset_buffer(bs);
		while ((len = BufferedSocket_peek(bs, &p)) > 0) {
			int rlen = 0;
			/* check if we have a complete line */
			char *end;
			if ((end = strchr(p, '\r')) == NULL)
				break;
			len = end - p;
			/* ok, we have a complete line */
			process_request(p, len, response, &rlen);
			/* done processing this line */
			/* FIXME: does not work with "^D" */
			BufferedSocket_discard(bs, len + strlen("\r\n"));
			/* send msg */
			len = BufferedSocket_write(bs, response, rlen);
			if (len <= 0)
				return;
		}
	}

}

static void listen_event(struct socket *lsock, void *data, int event_flags)
{
	static int client_counter = 0;
	struct socket *csock;
	int error;
	BufferedSocket *bs;

	if (event_flags & DATA_READY) {
		while (1) {
			error = sock_create_lite(PF_INET, SOCK_STREAM, IPPROTO_TCP, &csock);
			if (error) return;
			csock->ops = lsock->ops;
			error = lsock->ops->accept(lsock, csock, O_NONBLOCK);
			if (error) break;

			bs = kmalloc(sizeof(*bs), GFP_KERNEL);
			if (bs == NULL) break;

			BufferedSocket_init(bs, csock, client_event,
					    (void *) (long) client_counter);
			client_counter++;
		}
		sock_release(csock);
	}
}

static int __init init_memcached(void)
{
	struct sockaddr_in myaddr;
	int error;

	memset(&myaddr, 0, sizeof(myaddr));
	myaddr.sin_addr.s_addr = INADDR_ANY;
	myaddr.sin_port = htons(PORT);
	myaddr.sin_family = AF_INET;

	error = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &listen_sock);
	if (error) goto fail;
	error = listen_sock->ops->bind(listen_sock, (struct sockaddr*) &myaddr,
				       sizeof(myaddr));
	if (error) goto fail;
	error = listen_sock->ops->listen(listen_sock, BACKLOG_SIZE);
	if (error) goto fail;

	register_sock(listen_sock, listen_event, NULL);

	return 0;
fail:
	if (listen_sock != NULL)
		sock_release(listen_sock);
	listen_sock = NULL;
	return error;
}

static void __exit exit_memcached(void)
{
	if (listen_sock != NULL) {
		unregister_sock(listen_sock);
		sock_release(listen_sock);
		/* TODO: Add a cache_destroy call here */
	}
	listen_sock = NULL;
}

module_init(init_memcached);
module_exit(exit_memcached);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Ben Blum");
MODULE_DESCRIPTION("Memcached Module");
