#include "ezmb.h"
#include <lua.h>
#include <lauxlib.h>
#include <pthread.h>
#include <zmq.h>
#include <stdio.h>

typedef struct {
    e_device_t *device;
    lua_State *L;
    int callback_ref;
} lua_e_device_t;

static void *lua_listen_thread(void *arg) {
    lua_e_device_t *ud = (lua_e_device_t *)arg;
    e_device_t *device = ud->device;
    device->running = true;

    zmq_pollitem_t items[] = {
        { device->south_sock, 0, ZMQ_POLLIN, 0 },
    };

    while (device->running) {
        int rc = zmq_poll(items, 1, 100);
        if (rc == -1) continue;

        if (items[0].revents & ZMQ_POLLIN) {
            zmq_msg_t topic_msg, data_msg;
            zmq_msg_init(&topic_msg);
            zmq_msg_init(&data_msg);

            if (zmq_msg_recv(&topic_msg, device->south_sock, 0) == -1 ||
                zmq_msg_recv(&data_msg, device->south_sock, 0) == -1) {
                zmq_msg_close(&topic_msg);
                zmq_msg_close(&data_msg);
                continue;
            }

            if (ud->callback_ref != LUA_NOREF && ud->L) {
                lua_rawgeti(ud->L, LUA_REGISTRYINDEX, ud->callback_ref);
                lua_pushlstring(ud->L, zmq_msg_data(&topic_msg), zmq_msg_size(&topic_msg));
                lua_pushlstring(ud->L, zmq_msg_data(&data_msg), zmq_msg_size(&data_msg));
                if (lua_pcall(ud->L, 2, 0, 0) != 0) {
                    fprintf(stderr, "[ERROR] Lua callback error: %s\n", lua_tostring(ud->L, -1));
                    lua_pop(ud->L, 1);
                }
            }

            zmq_msg_close(&topic_msg);
            zmq_msg_close(&data_msg);
        }
    }

    return NULL;
}

static int l_device_create(lua_State *L) {
    const char *uid = luaL_checkstring(L, 1);
    const char *south_url = luaL_checkstring(L, 2);
    const char *north_url = luaL_checkstring(L, 3);
    const char *topic = luaL_optstring(L, 4, NULL);

    lua_e_device_t *ud = (lua_e_device_t *)lua_newuserdata(L, sizeof(lua_e_device_t));
    ud->device = e_device_create(uid, south_url, north_url, topic, NULL);
    ud->L = L;
    ud->callback_ref = LUA_NOREF;

    if (!ud->device) {
        return luaL_error(L, "[ERROR] Failed to create e_device");
    }

    luaL_getmetatable(L, "e_device");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_device_set_callback(lua_State *L) {
    lua_e_device_t *ud = (lua_e_device_t *)luaL_checkudata(L, 1, "e_device");

    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    if (ud->callback_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, ud->callback_ref);
    }
    ud->callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

static int l_device_listen(lua_State *L) {
    lua_e_device_t *ud = (lua_e_device_t *)luaL_checkudata(L, 1, "e_device");
    pthread_t tid;
    pthread_create(&tid, NULL, lua_listen_thread, ud);
    pthread_detach(tid);
    return 0;
}

static int l_device_send(lua_State *L) {
    lua_e_device_t *ud = (lua_e_device_t *)luaL_checkudata(L, 1, "e_device");
    size_t len;
    const char *msg = luaL_checklstring(L, 2, &len);
    int ret = e_device_send(ud->device, msg, len);
    lua_pushboolean(L, ret == 0);
    return 1;
}

static int l_device_stop(lua_State *L) {
    lua_e_device_t *ud = (lua_e_device_t *)luaL_checkudata(L, 1, "e_device");
    e_device_stop(ud->device);
    return 0;
}

static int l_device_destroy(lua_State *L) {
    lua_e_device_t *ud = (lua_e_device_t *)luaL_checkudata(L, 1, "e_device");
    if (ud->callback_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, ud->callback_ref);
        ud->callback_ref = LUA_NOREF;
    }
    if (ud->device) {
        e_device_destroy(ud->device);
        ud->device = NULL;
    }
    return 0;
}

// 方法表
static const struct luaL_Reg e_device_methods[] = {
    {"listen", l_device_listen},
    {"send", l_device_send},
    {"stop", l_device_stop},
    {"destroy", l_device_destroy},
    {"set_callback", l_device_set_callback},
    {NULL, NULL}
};

// 元表
static const struct luaL_Reg e_device_meta[] = {
    {"__gc", l_device_destroy},
    {NULL, NULL}
};

// 模块函数
static const struct luaL_Reg e_device_lib[] = {
    {"create", l_device_create},
    {NULL, NULL}
};

int luaopen_e_device(lua_State *L) {

    luaL_newmetatable(L, "e_device");
    lua_newtable(L);
    for (const luaL_Reg *l = e_device_methods; l->name; l++) {
        lua_pushcfunction(L, l->func);
        lua_setfield(L, -2, l->name);
    }
    lua_setfield(L, -2, "__index");

    for (const luaL_Reg *l = e_device_meta; l->name; l++) {
        lua_pushcfunction(L, l->func);
        lua_setfield(L, -2, l->name);
    }

    lua_pop(L, 1);

    lua_newtable(L);
    for (const luaL_Reg *l = e_device_lib; l->name; l++) {
        lua_pushcfunction(L, l->func);
        lua_setfield(L, -2, l->name);
    }

    lua_pushstring(L, EZMB_DEFAULT_SOUTH_URL);
    lua_setfield(L, -2, "default_south_url");

    lua_pushstring(L, EZMB_DEFAULT_NORTH_URL);
    lua_setfield(L, -2, "default_north_url");

    return 1;
}
