### LUA插件

```lua
local driver = {}

driver.north = function(input, size)
    return input:reverse()
end

driver.south = function(input, size)
    local out = {}
    for i = 1, size do
        local byte = input:byte(i)
        out[i] = string.char((byte + 1) % 256)
    end
    return table.concat(out)
end

function register_plugin(plugin)
    plugin.north_transform = driver.north
    plugin.south_transform = driver.south
    return plugin
end
```

### ELF插件

```c
#include <stdlib.h>
#include <string.h>

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
 * @brief 北向转换函数
 * 
 * @param ud 插件上下文
 * @param in 输入数据
 * @param len 输入数据大小
 * @param out 输出数据
 */
static void north(void *ud, const void *in, size_t len, void **out, size_t *out_len) {
    *out = malloc(len);
    for (size_t i = 0; i < len; ++i)
        ((char *)*out)[i] = ((char *)in)[len - i - 1];
    *out_len = len;
}

/**
 * @brief 南向转换函数
 * 
 * @param ud 插件上下文
 * @param in 输入数据
 * @param len 输入数据大小
 * @param out 输出数据
 */
static void south(void *ud, const void *in, size_t len, void **out, size_t *out_len) {
    *out = malloc(len);
    for (size_t i = 0; i < len; ++i)
        ((char *)*out)[i] = ((char *)in)[i] + 1;
    *out_len = len;
}

/**
 * @brief 注册插件
 * 
 * @param driver 插件驱动
 * @return 0 成功
 * 
 * PS: 必须命名函数为register_plugin
 */
int register_plugin(e_plugin_driver_t *driver) {
    driver->north_transform = north;
    driver->south_transform = south;
    return 0;
}
```