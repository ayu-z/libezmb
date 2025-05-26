#ifndef E_TCP_SERVER_H
#define E_TCP_SERVER_H

#include <event2/event.h>

#define E_TCP_SERVER_CLIENT_CAPACITY 10

/**
 * @brief 回调类型
 * 
 * 
 */
typedef void (*e_tcp_server_rev_callback)(const void *payload, size_t size, void *data);

/**
 * @brief TCP 服务器结构体
 * 
 * 
 */
typedef struct e_tcp_server {
    struct evconnlistener *listener; // 监听器实例
    struct sockaddr_in sin;          // 服务器地址
    int port;
    e_tcp_server_rev_callback read_cb; // 读取回调函数
    struct bufferevent **clients;     // 客户端列表
    int client_count;                // 客户端数量
    int client_capacity;             // 客户端列表容量
} e_tcp_server_t;

/**
 * @brief 发送参数结构体
 * 
 * 
 */
typedef struct {
    e_tcp_server_t *server;
    const void *payload;
    size_t size;
} send_arg_t;



/**
 * @brief 创建 TCP 服务器
 * @param base 事件基础结构体
 * @param port 端口号
 * @param cb 回调函数
 * @return 返回 TCP 服务器结构体
 */
e_tcp_server_t *e_tcp_server_create(struct event_base *base, int port, e_tcp_server_rev_callback cb);

/**
 * @brief 启动事件循环（如果需要）
 * @param server 服务器结构体
 * @return 返回 0 表示成功，-1 表示失败
 */
int e_tcp_server_dispatch(e_tcp_server_t *server);

/**
 * @brief 广播消息
 * @param server 服务器结构体
 * @param msg 消息
 * @param len 消息长度
 */
void e_tcp_server_broadcast(e_tcp_server_t *server, const void *payload, size_t size);

/**
 * @brief 释放服务器资源
 * @param server 服务器结构体
 */
void e_tcp_server_free(e_tcp_server_t *server);



#endif // E_TCP_SERVER_H
