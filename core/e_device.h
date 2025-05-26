#ifndef E_DEVICE_H
#define E_DEVICE_H

#include <stddef.h>
#include <stdbool.h>

/**
 * @brief 设备类型
 * 
 * @param E_DEVICE_TYPE_COLLECTOR: 采集器
 * @param E_DEVICE_TYPE_MONITOR: 监视器
 */
typedef enum {
    E_DEVICE_TYPE_COLLECTOR = 0,    
    E_DEVICE_TYPE_MONITOR = 1,      
}e_device_type_t;

/**
 * @brief 创建采集器
 * 
*/
#define e_collector_create(uid, south_url, north_url, cb) e_common_create(uid, south_url, north_url, cb, E_DEVICE_TYPE_COLLECTOR)

/**
 * @brief 创建监视器
 * 
*/
#define e_monitor_create(uid, south_url, north_url, cb) e_common_create(uid, south_url, north_url, cb, E_DEVICE_TYPE_MONITOR)

/**
 * @brief 创建默认采集器
 * 
*/
#define e_collector_create_default(uid, cb) e_collector_create(uid, EZMB_DEFAULT_SOUTH_URL, EZMB_DEFAULT_NORTH_URL, cb)

/**
 * @brief 创建默认监视器
 * 
*/
#define e_monitor_create_default(uid, cb) e_monitor_create(uid, EZMB_DEFAULT_SOUTH_URL, EZMB_DEFAULT_NORTH_URL, cb)

/**
 * @brief 发送消息
 * 
*/
#define e_collector_send     e_common_send  
#define e_monitor_send   e_common_send  

/**
 * @brief 监听设备
 * 
*/
#define e_collector_listen e_device_listen
#define e_monitor_listen e_device_listen

/**
 * @brief 停止设备
 * 
*/
#define e_collector_stop e_device_stop
#define e_monitor_stop e_device_stop

/**
 * @brief 销毁设备
 * 
*/
#define e_collector_destroy e_device_destroy
#define e_monitor_destroy e_device_destroy


/* 设备接收回调函数类型 */
typedef void (*e_device_recv_cb)(const char *topic, size_t topic_len, const void *payload, size_t payload_len, void *data);
#define e_device_monitor_cb e_device_recv_cb

/**
 * @brief 设备结构体
 * 
*/
typedef struct {
    void *ctx;              //zmq上下文
    e_device_type_t type;   //设备类型
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
 * @param type 设备类型
 * @return 设备句柄
 */
e_device_t *e_common_create(const char *uid, const char *south_url, const char *north_url, e_device_recv_cb cb, e_device_type_t type);


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
int e_common_send(e_device_t *device, const char *msg, size_t size);

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
