#ifndef E_DEVICE_H
#define E_DEVICE_H

#include <stddef.h>
#include <stdbool.h>

/* 设备接收回调函数类型 */
typedef void (*e_device_recv_cb)(const char *topic, size_t topic_len, const char *payload, size_t payload_len, void *data);

/* 设备结构体 */
typedef struct {
    void *ctx;              //zmq上下文
    void *south_sock;       //南向socket
    void *north_sock;       //北向socket
    char *uid;              //设备ID
    char *south_url;        //南向地址
    char *north_url;        //北向地址
    char *south_topic;      //南向主题
    char *north_topic;      //北向主题
    e_device_recv_cb cb;    //回调函数
    bool running;//运行状态
} e_device_t;

/**
 * @brief 创建设备
 * @param uid 设备ID
 * @param south_url 南向地址
 * @param north_url 北向地址
 * @param cb 回调函数
 * @return 设备句柄
 */
e_device_t *e_device_create(const char *uid, const char *south_url, const char *north_url, e_device_recv_cb cb);

/**
 * @brief 监听设备
 * @param device 设备句柄
 */
void e_device_listen(e_device_t *device);

/**
 * @brief 发送消息
 * @param device 设备句柄
 * @param msg 消息
 * @param size 消息大小
 */
int e_device_send(e_device_t *device, const char *msg, size_t size);

/**
 * @brief 停止设备
 * @param device 设备句柄
 */
void e_device_stop(e_device_t *device);

/**
 * @brief 销毁设备
 * @param device 设备句柄
 */
void e_device_destroy(e_device_t *device);

#endif // E_DEVICE_H
