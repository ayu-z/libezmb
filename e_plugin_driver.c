#include "e_plugin_driver.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdbool.h>

// 不放在头文件中，避免引用头文件需要加载lua库

/**
 * @brief lua插件
 */
typedef struct {
    lua_State *L;
    int north_ref;
    int south_ref;
} lua_plugin_t;

/**
 * @brief elf插件
 */
typedef struct {
    void *handle;
} elf_plugin_t;

/**
 * @brief 插件上下文
 */
typedef struct e_plugin_context {
    e_plugin_type_t type;
    e_plugin_driver_t driver;

    /**
     * @brief 插件实现
     */
    union {
        lua_plugin_t lua;
        elf_plugin_t elf;
    } impl;
} e_plugin_context_t;

/**
 * @brief 判断文件是否为elf文件
 * 
 * @param filename 文件名
 * @return 是否为elf文件
 */
static bool file_is_elf(const char *filename) {
    const char *elf_magic = "\x7f""ELF";  //so库文件的头部
    unsigned char header[4];
    FILE *f = fopen(filename, "rb");
    if (!f) return false;
    if (fread(header, 1, 4, f) != 4) {
        fclose(f);
        return false;
    }
    fclose(f);
    return (memcmp(header, elf_magic, 4) == 0);
}

/**
 * @brief 调用lua引用
 * 
 * @param L lua状态
 * @param ref 引用
 * @param in_data 输入数据
 * @param in_size 输入数据大小
 * @param out_data 输出数据
 * @param out_size 输出数据大小
 */
static void call_lua_ref(lua_State *L, int ref,
                         const void *in_data, size_t in_size,
                         void **out_data, size_t *out_size) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    lua_pushlstring(L, (const char *)in_data, in_size);
    lua_pushinteger(L, in_size);

    if (lua_pcall(L, 2, 1, 0) != 0) {
        fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        *out_data = NULL;
        *out_size = 0;
        return;
    }

    size_t len = 0;
    const char *res = lua_tolstring(L, -1, &len);
    if (!res) {
        *out_data = NULL;
        *out_size = 0;
        lua_pop(L, 1);
        return;
    }

    *out_data = malloc(len);
    memcpy(*out_data, res, len);
    *out_size = len;
    lua_pop(L, 1);
}

/**
 * @brief lua北向转换回调函数
 * 
 * @param ctx 插件上下文
 * @param in_data 输入数据
 * @param in_size 输入数据大小
 * @param out_data 输出数据
 * @param out_size 输出数据大小
 */
static void lua_north_cb(void *ctx,
                         const void *in_data, size_t in_size,
                         void **out_data, size_t *out_size) {
    e_plugin_context_t *plugin = (e_plugin_context_t *)ctx;
    call_lua_ref(plugin->impl.lua.L, plugin->impl.lua.north_ref, in_data, in_size, out_data, out_size);
}

/**
 * @brief lua南向转换回调函数
 * 
 * @param ctx 插件上下文
 * @param in_data 输入数据
 * @param in_size 输入数据大小
 * @param out_data 输出数据
 * @param out_size 输出数据大小
 */
static void lua_south_cb(void *ctx,
                         const void *in_data, size_t in_size,
                         void **out_data, size_t *out_size) {
    e_plugin_context_t *plugin = (e_plugin_context_t *)ctx;
    call_lua_ref(plugin->impl.lua.L, plugin->impl.lua.south_ref, in_data, in_size, out_data, out_size);
}

e_plugin_context_t *e_plugin_load_from_lua_script(const char *filename) {
    if (!filename) return NULL;

    e_plugin_context_t *ctx = calloc(1, sizeof(*ctx));
    ctx->type = PLUGIN_NONE;
    ctx->impl.lua.L = luaL_newstate();
    luaL_openlibs(ctx->impl.lua.L);

    if (luaL_dofile(ctx->impl.lua.L, filename) != 0) {
        fprintf(stderr, "Failed to load Lua script: %s\n", lua_tostring(ctx->impl.lua.L, -1));
        lua_close(ctx->impl.lua.L);
        free(ctx);
        return NULL;
    }

    lua_getglobal(ctx->impl.lua.L, "register_plugin");
    if (!lua_isfunction(ctx->impl.lua.L, -1)) {
        fprintf(stderr, "register_plugin not found in Lua script\n");
        lua_close(ctx->impl.lua.L);
        free(ctx);
        return NULL;
    }

    lua_newtable(ctx->impl.lua.L); 

    if (lua_pcall(ctx->impl.lua.L, 1, 1, 0) != 0) {
        fprintf(stderr, "register_plugin() error: %s\n", lua_tostring(ctx->impl.lua.L, -1));
        lua_close(ctx->impl.lua.L);
        free(ctx);
        return NULL;
    }

    if (!lua_istable(ctx->impl.lua.L, -1)) {
        fprintf(stderr, "register_plugin() did not return a table\n");
        lua_close(ctx->impl.lua.L);
        free(ctx);
        return NULL;
    }

    lua_getfield(ctx->impl.lua.L, -1, "north_transform");
    if (!lua_isfunction(ctx->impl.lua.L, -1)) {
        lua_pop(ctx->impl.lua.L, 2); 
        fprintf(stderr, "Lua: north_transform not function\n");
        lua_close(ctx->impl.lua.L);
        free(ctx);
        return NULL;
    }
    ctx->impl.lua.north_ref = luaL_ref(ctx->impl.lua.L, LUA_REGISTRYINDEX); 

    lua_getfield(ctx->impl.lua.L, -1, "south_transform");
    if (!lua_isfunction(ctx->impl.lua.L, -1)) {
        lua_pop(ctx->impl.lua.L, 1); 
        fprintf(stderr, "Lua: south_transform not function\n");
        lua_close(ctx->impl.lua.L);
        free(ctx);
        return NULL;
    }
    ctx->impl.lua.south_ref = luaL_ref(ctx->impl.lua.L, LUA_REGISTRYINDEX); 

    lua_pop(ctx->impl.lua.L, 1);

    ctx->driver.north_transform = lua_north_cb;
    ctx->driver.south_transform = lua_south_cb;
    ctx->type = PLUGIN_LUA;

    return ctx;
}



e_plugin_context_t *e_plugin_load_from_elf(const char *filename) {
    if (!filename) return NULL;

    e_plugin_context_t *ctx = calloc(1, sizeof(*ctx));
    ctx->type = PLUGIN_NONE;
    void *handle = dlopen(filename, RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "Failed to load .so plugin: %s\n", dlerror());
        return NULL;
    }

    typedef int (*register_plugin_func)(e_plugin_driver_t *);
    register_plugin_func reg = (register_plugin_func)dlsym(handle, "register_plugin");
    if (!reg) {
        fprintf(stderr, "No register_plugin found: %s\n", dlerror());
        dlclose(handle);
        e_plugin_destroy(ctx);
        return NULL;
    }

    if (reg(&ctx->driver) != 0) {
        fprintf(stderr, "register_plugin failed.\n");
        dlclose(handle);
        e_plugin_destroy(ctx);
        return NULL;
    }

    ctx->impl.elf.handle = handle;
    ctx->type = PLUGIN_ELF;
    return ctx;
}

e_plugin_context_t *e_plugin_create(const char *filename) {
    if (!filename) return NULL;
    e_plugin_context_t *ctx = calloc(1, sizeof(*ctx));
    ctx->type = PLUGIN_NONE;
    if (file_is_elf(filename)) {
        ctx = e_plugin_load_from_elf(filename);
    } else {
        ctx = e_plugin_load_from_lua_script(filename);
    }
    return ctx;
}

void e_plugin_destroy(e_plugin_context_t *ctx) {
    if (!ctx) return;

    if (ctx->type == PLUGIN_LUA && ctx->impl.lua.L) {
        lua_close(ctx->impl.lua.L);
    }

    if (ctx->type == PLUGIN_ELF && ctx->impl.elf.handle) {
        dlclose(ctx->impl.elf.handle);
    }

    free(ctx);
}

e_plugin_driver_t *e_plugin_load_driver(e_plugin_context_t *ctx) {
    if (!ctx) return NULL;
    return &ctx->driver;
}

e_plugin_type_t e_plugin_type(e_plugin_context_t *ctx) {
    if (!ctx) return PLUGIN_NONE;
    return ctx->type;
}