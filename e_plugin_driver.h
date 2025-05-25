#ifndef E_PLUGIN_DRIVER_H
#define E_PLUGIN_DRIVER_H

#include <stddef.h>
#include <stdbool.h>

/**
 * @brief 插件类型
 */
typedef enum {
    PLUGIN_NONE,
    PLUGIN_LUA,
    PLUGIN_ELF
} e_plugin_type_t;


/**
 * @brief 数据转换回调函数
 * 
 * @param ctx 插件上下文
 * @param in_data 输入数据
 * @param in_data_size 输入数据大小
 * @param out_data 输出数据
 * @param out_data_size 输出数据大小
 */
typedef void (*e_plugin_message_transform_callback)(
    void *ctx,
    const void *in_data,
    size_t in_data_size,
    void **out_data,
    size_t *out_data_size
);

/**
 * @brief 插件驱动
 * 
 * @param north_transform 北向转换回调函数
 * @param south_transform 南向转换回调函数
 */
typedef struct {
    e_plugin_message_transform_callback north_transform;
    e_plugin_message_transform_callback south_transform;
} e_plugin_driver_t;

/**
 * @brief 插件上下文
 */
typedef struct e_plugin_context e_plugin_context_t;

/**
 * @brief 创建插件上下文
 * 
 * @param filename 插件文件名 支持lua脚本和.so文件
 * @return 插件上下文
 */
e_plugin_context_t *e_plugin_create(const char *filename);

/**
 * @brief 销毁插件上下文
 * 
 * @param ctx 插件上下文
 */
void e_plugin_destroy(e_plugin_context_t *ctx);

/**
 * @brief 从lua脚本创建插件上下文
 * 
 * @param filename 插件文件名
 * @return 插件上下文
 */
e_plugin_context_t *e_plugin_load_from_lua_script(const char *filename);

/**
 * @brief 从.so文件创建插件上下文
 * 
 * @param filename 插件文件名
 * @return 插件上下文
 */
e_plugin_context_t *e_plugin_load_from_elf(const char *filename);

/**
 * @brief 加载插件驱动
 * 
 * @param ctx 插件上下文
 * @return 插件驱动
 */
const e_plugin_driver_t *e_plugin_load_driver(e_plugin_context_t *ctx);

/**
 * @brief 获取插件类型
 * 
 * @param ctx 插件上下文
 * @return 插件类型
 */
e_plugin_type_t e_plugin_type(e_plugin_context_t *ctx);
#endif
