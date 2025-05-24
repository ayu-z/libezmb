#ifndef E_SERIALPORT_H
#define E_SERIALPORT_H

#include "e_config.h"

/* 串口接收回调函数类型 */
typedef void (*serial_recv_callback_t)(void *ctx, const char *data, size_t len);

/* 串口上下文 */
typedef struct {
    int fd;
    volatile sig_atomic_t running;  // 线程运行标志
    pthread_mutex_t fd_mutex;       // 保护 fd 的互斥锁
    serial_recv_callback_t recv_cb; // 接收回调函数
    serial_config_t ser;            // 串口配置
    void *data;                     // 回调函数参数
} serial_context_t;

/**
 * @brief 配置串口
 * @param fd 串口文件描述符
 * @param cfg 串口配置
 * @return 0成功，-1失败
 */
int serial_configure(int fd, const serial_config_t *cfg);

/**
 * @brief 打开串口
 * @param cfg 串口配置
 * @return 文件描述符，-1失败
 */
int serial_open(const serial_config_t *cfg);

/**
 * @brief 关闭串口
 * @param fd 串口文件描述符
 */
void serial_close(int *fd);

/**
 * @brief 写入数据
 * @param ctx 串口上下文
 * @param data 数据
 * @param len 数据长度
 * @return 写入的字节数
 */
size_t serial_write(serial_context_t *ctx, const char *data, size_t len);

/**
 * @brief 设置接收回调函数
 * @param ctx 串口上下文
 * @param cb 接收回调函数
 * @return 0成功，-1失败
 */
int serial_set_recv_callback(serial_context_t *ctx, serial_recv_callback_t cb);

#endif // E_SERIALPORT_H