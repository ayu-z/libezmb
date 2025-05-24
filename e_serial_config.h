#ifndef E_CONFIG_H
#define E_CONFIG_H

#include <pthread.h>
#include <signal.h>
#include "e_queue.h"
#define DEFAULT_BUF_MAX             1024
#define DEFAULT_SERIAL_BAUDRATE     115200UL
#define DEFAULT_SERIAL_REV_TIMEOUT  100
#define MAX_DEVICE_NAME_LEN         32

/* 串口配置 */
typedef struct {
    char *uid;         // 设备唯一ID
    char *device;      // 设备名称
    int baudrate;      // 波特率
    int databits;      // 数据位
    int stopbits;      // 停止位
    char parity;       // 校验位
    int min_delay_ms;  // 最小发送间隔 (ms)
    int maxlen;        // 最大接收缓冲区长度
    int timeout_ms;    // 接收超时时间
} serial_config_t;

/**
 * @brief 解析命令行参数
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @param config 串口配置
 * @return 0成功，-1失败
 */
int e_config_parse_args(int argc, char **argv, e_queue_t *g_port_queue);

/**
 * @brief 打印串口配置
 * @param config 串口配置
 */
void e_config_print(const serial_config_t *config);

/**
 * @brief 打印使用说明
 * @param prog 程序名
 */
void e_config_print_usage(const char *prog);
#endif // E_CONFIG_H
