#ifndef E_TCP_SERVER_H
#define E_TCP_SERVER_H

#include <event2/event.h>

#ifdef __cplusplus
extern "C" {
#endif

// 定义 tcp_server 结构体
typedef struct e_tcp_server {
    struct evconnlistener *listener; // 监听器实例
    struct sockaddr_in sin;          // 服务器地址
    int port;
    void (*read_cb)(const void *msg); // 读取回调函数
    struct bufferevent **clients;     // 客户端列表
    int client_count;                // 客户端数量
    int client_capacity;             // 客户端列表容量
} e_tcp_server_t;



// 回调类型
typedef void (*e_tcp_server_rev_callback)(const void *msg);

// 创建 TCP 服务器
e_tcp_server_t *e_tcp_server_create(struct event_base *base, int port, e_tcp_server_rev_callback cb);

// 启动事件循环（如果需要）
int e_tcp_server_dispatch(e_tcp_server_t *server);

// 广播消息
void e_tcp_server_broadcast(e_tcp_server_t *server, const void *msg, size_t len);

// 释放服务器资源
void e_tcp_server_free(e_tcp_server_t *server);

#ifdef __cplusplus
}
#endif

#endif // E_TCP_SERVER_H
