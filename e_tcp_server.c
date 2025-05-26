#include "e_tcp_server.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
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
static const int PORT = 9995;

static char g_szReadMsg[10240] = {0};

static void listener_cb(struct evconnlistener *, evutil_socket_t,
    struct sockaddr *, int socklen, void *);
    
static void conn_readcb(struct bufferevent *, void *);
static void conn_eventcb(struct bufferevent *, short, void *);

typedef struct {
    e_tcp_server_t *server;
    const void *msg;
    size_t len;
} send_arg_t;

static void hexdump(const char *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (i % 8 == 0) {
            printf("%04zX: ", i);
        }
        printf("%02X ", (unsigned char)data[i]);
        if (i % 8 == 7 || i == len - 1) {
            if (i % 8 != 7) {
                for (size_t j = 0; j < (7 - (i % 8)); j++) {
                    printf("   ");
                }
            }
            printf(" | ");
            size_t line_start = (i / 8) * 8;
            size_t line_end = line_start + 8;
            for (size_t j = line_start; j < line_end; j++) {
                if (j < len) {
                    unsigned char c = data[j];
                    printf("%c", (c >= 32 && c <= 126) ? c : '.');
                } else {
                    printf(" ");
                }
            }
            printf("\n");
        }
    }
}



// 创建并初始化 tcp_server
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
    server->client_capacity = 10;
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
    const void  *msg = send_arg->msg;
    size_t len = send_arg->len;
    bufferevent_write(bev, msg, len);
    return NULL;
}

// 向所有客户端发送数据
void e_tcp_server_broadcast(e_tcp_server_t *server, const void *msg, size_t len) {
    send_arg_t *arg = (send_arg_t *)malloc(sizeof(send_arg_t));
    if (!arg) {
        perror("malloc failed");
        return;
    }

    arg->server = server;
    arg->msg = malloc(len);  // 拷贝数据，确保线程安全
    if (!arg->msg) {
        perror("malloc for msg failed");
        free(arg);
        return;
    }
    memcpy((void *)arg->msg, msg, len);
    arg->len = len;

    for (int i = 0; i < server->client_count; i++) {
        pthread_t tid;
        pthread_create(&tid, NULL, e_tcp_server_send, arg);
        pthread_detach(tid);  // 避免线程泄漏
    }
    
}


// 移除客户端
void e_tcp_server_remove_client(e_tcp_server_t *server, struct bufferevent *bev) {
    for (int i = 0; i < server->client_count; i++) {
        if (server->clients[i] == bev) {
            server->clients[i] = server->clients[server->client_count - 1];
            server->client_count--;
            break;
        }
    }
}

// 示例读取回调函数
void example_read_cb(const void *msg) {
    printf("Custom read callback: \n");
    hexdump(msg, strlen(msg));
}

// 主函数
int main(int argc, char **argv) {
    struct event_base *base;

    base = event_base_new();
    if (!base) {
        fprintf(stderr, "Could not initialize libevent!\n");
        return 1;
    }

    // 创建 TCP 服务器实例，并传入自定义的 read_cb 回调函数
    e_tcp_server_t *server = e_tcp_server_create(base, PORT, example_read_cb);

    if (!server) {
        fprintf(stderr, "Could not create server!\n");
        event_base_free(base);
        return 1;
    }

    // 进入事件循环
    event_base_dispatch(base);

    // 释放资源
    evconnlistener_free(server->listener);
    free(server->clients);
    free(server);
    event_base_free(base);

    printf("done\n");
    return 0;
}

// 监听回调函数
static void listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *sa, int socklen, void *user_data) {
    e_tcp_server_t *server = (e_tcp_server_t *)user_data;
    struct event_base *base = evconnlistener_get_base(listener);
    struct bufferevent *bev;

    bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        fprintf(stderr, "Error constructing bufferevent!");
        event_base_loopbreak(base);
        return;
    }

    // 将 tcp_server_t 实例传递给 bufferevent 的回调函数
    bufferevent_setcb(bev, conn_readcb, NULL, conn_eventcb, server);
    bufferevent_enable(bev, EV_WRITE);
    bufferevent_enable(bev, EV_READ);

    // 如果客户端列表已满，扩容
    if (server->client_count >= server->client_capacity) {
        server->client_capacity *= 2;
        server->clients = (struct bufferevent **)realloc(server->clients, server->client_capacity * sizeof(struct bufferevent *));
        if (!server->clients) {
            fprintf(stderr, "Failed to reallocate memory for clients\n");
            return;
        }
    }

    // 将新客户端添加到客户端列表
    server->clients[server->client_count++] = bev;
}

// 读取回调函数
static void conn_readcb(struct bufferevent *bev, void *user_data) {
    e_tcp_server_t *server = (e_tcp_server_t *)user_data;
    memset(g_szReadMsg, 0x00, sizeof(g_szReadMsg));
    struct evbuffer *input = bufferevent_get_input(bev);
    size_t sz = evbuffer_get_length(input);
    if (sz > 0) {
        bufferevent_read(bev, g_szReadMsg, sz);
        printf("cli:>>\n");
        hexdump(g_szReadMsg, sz);

        // 调用自定义的 read_cb 回调函数
        if (server->read_cb) {
            server->read_cb(g_szReadMsg);
        }

        e_tcp_server_broadcast(server, g_szReadMsg, sz);
    }
}

// 事件回调函数
static void conn_eventcb(struct bufferevent *bev, short events, void *user_data) {
    e_tcp_server_t *server = (e_tcp_server_t *)user_data;
    if (events & BEV_EVENT_EOF) {
        printf("Connection closed.\n");
    } else if (events & BEV_EVENT_ERROR) {
        printf("Got an error on the connection: %s\n", strerror(errno));
    }
    // 从客户端列表中移除
    e_tcp_server_remove_client(server, bev);
    bufferevent_free(bev);
}