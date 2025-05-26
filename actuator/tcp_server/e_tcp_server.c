#include "e_tcp_server.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

#include <stdlib.h>
#include <pthread.h>

static void listener_cb(struct evconnlistener *, evutil_socket_t,
    struct sockaddr *, int socklen, void *);
    
static void conn_readcb(struct bufferevent *, void *);
static void conn_eventcb(struct bufferevent *, short, void *);


e_tcp_server_t *e_tcp_server_create(struct event_base *base, int port, e_tcp_server_rev_callback cb) {
    e_tcp_server_t *server = (e_tcp_server_t *)malloc(sizeof(e_tcp_server_t));
    if (!server) {
        fprintf(stderr, "Failed to allocate memory for server\n");
        return NULL;
    }

    memset(&server->sin, 0, sizeof(server->sin));
    server->sin.sin_family = AF_INET;
    server->sin.sin_port = htons(port);

    server->listener = evconnlistener_new_bind(base, listener_cb, (void *)server,
        LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
        (struct sockaddr*)&server->sin,
        sizeof(server->sin));

    if (!server->listener) {
        fprintf(stderr, "Could not create a listener!\n");
        free(server);
        return NULL;
    }

    server->port = port;
    server->read_cb = cb;
    server->client_count = 0;
    server->client_capacity = E_TCP_SERVER_CLIENT_CAPACITY;
    server->clients = (struct bufferevent **)malloc(server->client_capacity * sizeof(struct bufferevent *));
    if (!server->clients) {
        fprintf(stderr, "Failed to allocate memory for clients\n");
        free(server);
        return NULL;
    }

    return server;
}

static void *e_tcp_server_send(void *arg) {
    send_arg_t *send_arg = (send_arg_t *)arg;
    e_tcp_server_t *server = send_arg->server;
    struct bufferevent *bev = server->clients[0];
    const void  *payload = send_arg->payload;
    size_t size = send_arg->size;
    bufferevent_write(bev, payload, size);
    return NULL;
}


void e_tcp_server_broadcast(e_tcp_server_t *server, const void *payload, size_t size) {
    send_arg_t *arg = (send_arg_t *)malloc(sizeof(send_arg_t));
    if (!arg) {
        perror("malloc failed");
        return;
    }

    arg->server = server;
    arg->payload = malloc(size);
    if (!arg->payload) {
        perror("malloc for payload failed");
        free(arg);
        return;
    }
    memcpy((void *)arg->payload, payload, size);
    arg->size = size;

    for (int i = 0; i < server->client_count; i++) {
        pthread_t tid;
        pthread_create(&tid, NULL, e_tcp_server_send, arg);
        pthread_detach(tid);
    }
    
}


void e_tcp_server_remove_client(e_tcp_server_t *server, struct bufferevent *bev) {
    for (int i = 0; i < server->client_count; i++) {
        if (server->clients[i] == bev) {
            server->clients[i] = server->clients[server->client_count - 1];
            server->client_count--;
            break;
        }
    }
}


static void listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *sa, int socklen, void *user_data) {
    (void)sa;
    (void)socklen;
    e_tcp_server_t *server = (e_tcp_server_t *)user_data;
    struct event_base *base = evconnlistener_get_base(listener);
    struct bufferevent *bev;

    bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        fprintf(stderr, "Error constructing bufferevent!");
        event_base_loopbreak(base);
        return;
    }

    bufferevent_setcb(bev, conn_readcb, NULL, conn_eventcb, server);
    bufferevent_enable(bev, EV_WRITE);
    bufferevent_enable(bev, EV_READ);

    if (server->client_count >= server->client_capacity) {
        server->client_capacity *= 2;
        server->clients = (struct bufferevent **)realloc(server->clients, server->client_capacity * sizeof(struct bufferevent *));
        if (!server->clients) {
            fprintf(stderr, "Failed to reallocate memory for clients\n");
            return;
        }
    }

    server->clients[server->client_count++] = bev;
}

static void conn_readcb(struct bufferevent *bev, void *user_data) {
    e_tcp_server_t *server = (e_tcp_server_t *)user_data;
    struct evbuffer *input = bufferevent_get_input(bev);
    size_t sz = evbuffer_get_length(input);

    if (sz > 0) {
        char *msg = (char *)malloc(sz);
        if (!msg) {
            perror("malloc for msg failed");
            return;
        }
        bufferevent_read(bev, msg, sz);

        if (server->read_cb) {
            server->read_cb(msg, sz, (void *)bev);
        }

        free(msg);
    }
}

static void conn_eventcb(struct bufferevent *bev, short events, void *user_data) {
    e_tcp_server_t *server = (e_tcp_server_t *)user_data;
    if (events & BEV_EVENT_EOF) {
        printf("Connection closed.\n");
    } else if (events & BEV_EVENT_ERROR) {
        printf("Got an error on the connection: %s\n", strerror(errno));
    }

    e_tcp_server_remove_client(server, bev);
    bufferevent_free(bev);
}