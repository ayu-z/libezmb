#ifndef E_TCP_SERVER_CONFIG_H
#define E_TCP_SERVER_CONFIG_H

#include <stdbool.h>

/**
 * @brief TCP 服务器配置
 * @param uid 监听设备ID
 * @param port 监听端口
 * @param plugin_path 插件路径
 */
typedef struct {
    char *uid;
    int port;
    bool plugin_enable;
    char *plugin_path;
} tcp_server_config_t;

/**
 * @brief 解析命令行参数
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @param config TCP 服务器配置
 * @return 0成功，-1失败
 */
bool e_tcp_server_config_parse(int argc, char **argv, tcp_server_config_t *config);

/**
 * @brief 打印 TCP 服务器配置
 * @param config TCP 服务器配置
 */
void e_tcp_server_config_print(const tcp_server_config_t *config);

/**
 * @brief 打印使用说明
 * @param prog 程序名
 */
void e_tcp_server_config_usage(const char *prog);
#endif // E_TCP_SERVER_CONFIG_H
