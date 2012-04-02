#include "../klibevent/klibevent.h"
#include "cache.h"

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/socket.h>
#include <net/sock.h>

// #include <linux/slab.h>

#define PORT 19620
#define BACKLOG_SIZE 10
#define BUFFER_LEN 512

#define MSG_PREFIX "[memcached] "

/*
 * Communication protocol:
 * msg       ::= "<readtype> <key>\r\n" | "<writetype> <key>\r\n<data>\r\n"
 * readtype  ::= 'r' | 'q'
 * key       ::= [a-zA-Z0-9]*
 * writetype ::= 'w' | 'i'
 * data      ::= [a-zA-Z0-9]*
 */

struct client_data {
    int id;
    char buffer[BUFFER_LEN];
    int off;
    int len;
};

struct socket *listen_sock;

static void client_event(struct socket *sock, void *data, int event_flags) {
    struct client_data *cd;
    struct msghdr msg;
    struct iovec iov;
    int len;
    cd = (struct client_data*) data;

    printk(MSG_PREFIX "Event on client sock %d = 0x%X\n", cd->id, event_flags);

    if(event_flags & STATE_CHANGED) {
        if(TCP_SOCK_CLOSED(sock)) {
            unregister_sock(sock);
            sock_release(sock);
            return;
        }
    }

    if((event_flags & DATA_READY) || (event_flags & WRITE_SPACE)) {
        char response[BUFFER_LEN];
        int response_start = 0;
	int rlen = 0;
	char *key;
	int keylen;
	char *data;
	int datalen;
	int ret;

        msg.msg_name = 0;
        msg.msg_namelen = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags = MSG_DONTWAIT;
    
        iov.iov_base = cd->buffer;
        iov.iov_len = BUFFER_LEN;
    
        /* read input */
        cd->len = sock_recvmsg(sock, &msg, BUFFER_LEN, MSG_DONTWAIT);
        if(cd->len <= 0) {
            cd->len = 0;
            return;
        }
        /* interpret and send reply */
	switch (cd->buffer[0]) {
		/* read/query action */
		case 'r':
		case 'q':
			printk(MSG_PREFIX "read len %d\n", cd->len);
			if (cd->buffer[1] != ' ') {
				rlen += snprintf(response+rlen, BUFFER_LEN-rlen,
						 "invalid format\r\n");
				break;
			}
			/* find key */
			key = &cd->buffer[2];
			keylen = strstr(key, "\r\n") - key;
			if (keylen <= 0) {
				rlen += snprintf(response+rlen, BUFFER_LEN-rlen,
						 "invalid key\r\n");
				break;
			}
			/* perform the requested operation */
			ret = cache_find(key, keylen, response+rlen,
					 &datalen, BUFFER_LEN-rlen);
			if (ret == 0) {
				rlen += datalen;
				rlen += snprintf(response+rlen, BUFFER_LEN-rlen,
						 "\r\nsuccess\r\n");
			} else {
				rlen += snprintf(response+rlen, BUFFER_LEN-rlen,
						 "fail\r\n");
			}
			break;
		/* write/insert action */
		case 'w':
		case 'i':
			printk(MSG_PREFIX "write len %d\r\n", cd->len);
			if (cd->buffer[1] != ' ') {
				rlen += snprintf(response+rlen, BUFFER_LEN-rlen,
						 "invalid format\r\n");
				break;
			}
			/* find key */
			key = &cd->buffer[2];
			keylen = strstr(key, " ") - key;
			if (keylen <= 0) {
				rlen += snprintf(response+rlen, BUFFER_LEN-rlen,
						 "invalid key\r\n");
				break;
			}
			/* find data */
			data = &key[keylen + 1];
			datalen = strstr(data, "\r\n") - data;
			if (data <= 0) {
				rlen += snprintf(response+rlen, BUFFER_LEN-rlen,
						 "invalid data\r\n");
				break;
			}
			/* perform the requested operation. */
			cache_insert(key, keylen, data, datalen);
			rlen += snprintf(response+rlen, BUFFER_LEN-rlen,
					 "success\r\n");
			printk(MSG_PREFIX "key of length %d with data of length %d "
					  "successfully inserted\r\n", keylen, datalen);
			break;
		/* invalid cmd */
		default:
			rlen += snprintf(response+rlen, BUFFER_LEN-rlen,
			                 "invalid cmd %x len %d\r\n", cd->buffer[0], cd->len);
	}
        while(cd->len - response_start > 0) {
            iov.iov_base = response + response_start;
            iov.iov_len = rlen - response_start;
            len = sock_sendmsg(sock, &msg, cd->len - cd->off);
            if(len <= 0) return;
            response_start += len;
        }
    }

}

static void listen_event(struct socket *lsock, void *data, int event_flags) {
    static int client_counter = 0;
    struct socket *csock;
    int error;
    struct client_data *cd;

    if(event_flags & DATA_READY) {
        while(1) {
            error = sock_create_lite(PF_INET, SOCK_STREAM, IPPROTO_TCP, &csock);
            if(error) return;
            csock->ops = lsock->ops;
            error = lsock->ops->accept(lsock, csock, O_NONBLOCK);
            if(error) break;
            cd = kmalloc(sizeof(*cd), GFP_KERNEL);
            if(cd == NULL) break;
            cd->id = client_counter++;
            cd->len = cd->off = 0;
            register_sock(csock, client_event, cd);
        }
        sock_release(csock);
    }
}

static int __init init_memcached(void) {
    struct sockaddr_in myaddr;  
    int error;
  
    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = htons(PORT);
    myaddr.sin_family = AF_INET;

    error = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &listen_sock);
    if(error) goto fail;
    error = listen_sock->ops->bind(listen_sock, (struct sockaddr*) &myaddr,
                                   sizeof(myaddr));
    if(error) goto fail;
    error = listen_sock->ops->listen(listen_sock, BACKLOG_SIZE);
    if(error) goto fail;
  
    register_sock(listen_sock, listen_event, NULL);

    return 0;

fail:
    if(listen_sock != NULL)
        sock_release(listen_sock);
    listen_sock = NULL;
    return error;
}

static void __exit exit_memcached(void) {
    if(listen_sock != NULL) {
        unregister_sock(listen_sock);
        sock_release(listen_sock);
    }
    listen_sock = NULL;
}



module_init(init_memcached);
module_exit(exit_memcached);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Ben Blum");
MODULE_DESCRIPTION("Memcached Module");


