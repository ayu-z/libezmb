#ifndef E_SERIAL_MANAGER_H
#define E_SERIAL_MANAGER_H

#include "e_queue.h"
#include "e_serialport.h"
#include <stdbool.h>


/* 串口接收回调函数类型 */
typedef void (*serial_recv_callback_t)(void *ctx, const char *data, size_t len);

/* 串口管理器结构体 */
typedef struct serial_manager {
    e_queue_t ports;              // 串口队列（使用e_queue管理）
    volatile sig_atomic_t running; // 原子运行标志
    pthread_mutex_t mutex;        // 全局互斥锁
    pthread_t read_tid;           // 读取线程
    pthread_t monitor_tid;        // 监控线程
} serial_manager_t;

/* ========== API 接口 ========== */

/**
 * @brief 创建串口管理器
 * @return 管理器句柄，失败返回NULL
 */
serial_manager_t *e_serial_manager_create(void);

/**
 * @brief 销毁串口管理器
 * @param manager 管理器句柄
 */
void e_serial_manager_destroy(serial_manager_t *manager);

/**
 * @brief 添加串口到管理器
 * @param manager 管理器句柄
 * @param config 串口配置
 * @param recv_cb 接收回调函数
 * @param data 回调函数参数
 * @return 成功返回0，失败返回-1
 */
int e_serial_manager_add_port(serial_manager_t *manager, 
                          const serial_config_t *config,
                          serial_recv_callback_t recv_cb,
                          void *data);

/**
 * @brief 从管理器移除串口
 * @param manager 管理器句柄
 * @param uid 要移除的设备uid
 */
void e_serial_manager_remove_port(serial_manager_t *manager, const char *uid);

/**
 * @brief 向指定串口写入数据
 * @param manager 管理器句柄
 * @param uid 目标串口uid
 * @param data 要写入的数据
 * @param len 数据长度
 * @return 成功写入的字节数，失败返回-1
 */
int e_serial_manager_write(serial_manager_t *manager, 
                       const char *uid,
                       const void *data, 
                       size_t len);

/**
 * @brief 启动串口管理器
 * @param manager 管理器句柄
 */
void e_serial_manager_start(serial_manager_t *manager);

/**
 * @brief 停止串口管理器
 * @param manager 管理器句柄
 */
void e_serial_manager_stop(serial_manager_t *manager);

/**
 * @brief 获取串口管理器中串口的数量
 * @param manager 管理器句柄
 * @return 串口数量
 */
int e_serial_manager_size(serial_manager_t *manager);


#endif // E_SERIAL_MANAGER_H