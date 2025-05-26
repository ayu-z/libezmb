#ifndef E_PROXY_H
#define E_PROXY_H

#include <stddef.h>
#include <stdbool.h>

/* 消息回调函数类型 */
typedef void (*e_message_callback)(const char *topic, size_t topic_size, const char *message, size_t size);

/* 代理结构体 */
typedef struct e_proxy {
    void *ctx;                  //zmq上下文
    void *frontend;             //前端socket
    void *backend;              //后端socket
    volatile bool running;      //运行状态
    e_message_callback callback;//消息回调函数
} e_proxy_t;

/**
 * @brief 创建代理
 * @param frontend 前端地址
 * @param backend 后端地址
 * @param callback 消息回调函数
 * @return 代理句柄
 */
e_proxy_t *e_proxy_create(const char *frontend, const char *backend, e_message_callback callback);

/**
 * @brief 销毁代理
 * @param proxy 代理句柄
 */
void e_proxy_destroy(e_proxy_t *proxy);

/**
 * @brief 代理监听
 * @param proxy 代理句柄
 */
void e_proxy_listen(e_proxy_t *proxy);

/**
 * @brief 代理停止
 * @param proxy 代理句柄
 */
void e_proxy_stop(e_proxy_t *proxy);



#endif // E_PROXY_H
