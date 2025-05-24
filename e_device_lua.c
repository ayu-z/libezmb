#include "ezmb.h"
#include <lua.h>
#include <lauxlib.h>
#include <pthread.h>
#include <zmq.h>
#include <stdio.h>
#include <string.h>

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 502
#define luaL_setfuncs(L, l, nup) luaL_register(L, NULL, l)
#define luaL_newlib(L, l) (lua_newtable(L), luaL_register(L, NULL, l))
#endif

typedef struct {
    e_device_t *device;
    lua_State *L;
    int callback_ref;
} lua_e_device_t;

static void *lua_listen_thread(void *arg) {
    lua_e_device_t *ud = (lua_e_device_t *)arg;
    e_device_t *device = ud->device;

    // const char *type_str = (device->type == E_DEVICE_TYPE_COLLECTOR) ? "COLLECTOR" :
    //                        (device->type == E_DEVICE_TYPE_MONITOR) ? "MONITOR" : "UNKNOWN";

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

            // printf("[%s] Received topic: %.*s, data: %.*s\n",
            //        type_str,
            //        (int)zmq_msg_size(&topic_msg), (const char *)zmq_msg_data(&topic_msg),
            //        (int)zmq_msg_size(&data_msg), (const char *)zmq_msg_data(&data_msg));

            if (ud->callback_ref != LUA_NOREF && ud->L) {
                lua_rawgeti(ud->L, LUA_REGISTRYINDEX, ud->callback_ref);
                lua_pushlstring(ud->L, (const char *)zmq_msg_data(&topic_msg), zmq_msg_size(&topic_msg));
                lua_pushlstring(ud->L, (const char *)zmq_msg_data(&data_msg), zmq_msg_size(&data_msg));
                if (lua_pcall(ud->L, 2, 0, 0) != 0) {
                    fprintf(stderr, "[%s] LUA callback error: %s\n", type_str, lua_tostring(ud->L, -1));
                    lua_pop(ud->L, 1);
                }
            }

            zmq_msg_close(&topic_msg);
            zmq_msg_close(&data_msg);
        }
    }

    return NULL;
}


static int l_collector_create(lua_State *L) {
    const char *uid = luaL_checkstring(L, 1);
    const char *south_url = luaL_optstring(L, 2, EZMB_DEFAULT_SOUTH_URL);
    const char *north_url = luaL_optstring(L, 3, EZMB_DEFAULT_NORTH_URL);

    lua_e_device_t *ud = (lua_e_device_t *)lua_newuserdata(L, sizeof(lua_e_device_t));
    ud->device = e_collector_create(uid, south_url, north_url, NULL);
    ud->device->running = true;
    ud->L = L;
    ud->callback_ref = LUA_NOREF;

    if (!ud->device) {
        return luaL_error(L, "Failed to create collector device");
    }

    luaL_getmetatable(L, "e_device");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_monitor_create(lua_State *L) {
    const char *uid = luaL_checkstring(L, 1);
    const char *south_url = luaL_optstring(L, 2, EZMB_DEFAULT_SOUTH_URL);
    const char *north_url = luaL_optstring(L, 3, EZMB_DEFAULT_NORTH_URL);

    lua_e_device_t *ud = (lua_e_device_t *)lua_newuserdata(L, sizeof(lua_e_device_t));
    ud->device = e_monitor_create(uid, south_url, north_url, NULL);
    ud->device->running = true;
    ud->L = L;
    ud->callback_ref = LUA_NOREF;

    if (!ud->device) {
        return luaL_error(L, "Failed to create monitor device");
    }

    luaL_getmetatable(L, "e_device");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_device_set_callback(lua_State *L) {
    lua_e_device_t *ud = (lua_e_device_t *)luaL_checkudata(L, 1, "e_device");
    luaL_checktype(L, 2, LUA_TFUNCTION);

    if (ud->callback_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, ud->callback_ref);
    }
    lua_pushvalue(L, 2);
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

    int ret = e_common_send(ud->device, msg, len);
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
    }

    if (ud->device) {
        e_device_destroy(ud->device);
        ud->device = NULL;
    }
    return 0;
}

static int l_device_index(lua_State *L) {
    lua_e_device_t *ud = (lua_e_device_t *)luaL_checkudata(L, 1, "e_device");
    const char *key = luaL_checkstring(L, 2);
    e_device_t *dev = ud->device;

    if (strcmp(key, "uid") == 0) {
        lua_pushstring(L, dev->uid);
        return 1;
    } else if (strcmp(key, "type") == 0) {
        lua_pushinteger(L, dev->type);
        return 1;
    } else if (strcmp(key, "south_url") == 0) {
        lua_pushstring(L, dev->south_url);
        return 1;
    } else if (strcmp(key, "north_url") == 0) {
        lua_pushstring(L, dev->north_url);
        return 1;
    } else if (strcmp(key, "south_topic") == 0) {
        lua_pushstring(L, dev->south_topic ? dev->south_topic : "");
        return 1;
    } else if (strcmp(key, "north_topic") == 0) {
        lua_pushstring(L, dev->north_topic ? dev->north_topic : "");
        return 1;
    } else if (strcmp(key, "running") == 0) {
        lua_pushboolean(L, dev->running);
        return 1;
    }

    luaL_getmetatable(L, "e_device");
    lua_getfield(L, -1, key);
    return 1;
}

static const luaL_Reg e_device_methods[] = {
    {"set_callback", l_device_set_callback},
    {"listen",       l_device_listen},
    {"send",         l_device_send},
    {"stop",         l_device_stop},
    {"destroy",      l_device_destroy},
    {NULL, NULL}
};

static const luaL_Reg e_device_meta[] = {
    {"__gc",    l_device_destroy},
    {"__index", l_device_index},
    {NULL, NULL}
};

static const luaL_Reg e_device_lib[] = {
    {"create_collector", l_collector_create},
    {"create_monitor",   l_monitor_create},
    {NULL, NULL}
};

int luaopen_e_device(lua_State *L) {
    luaL_newmetatable(L, "e_device");
    luaL_setfuncs(L, e_device_meta, 0);
    luaL_setfuncs(L, e_device_methods, 0);
    lua_pop(L, 1);

    luaL_newlib(L, e_device_lib);

    lua_pushstring(L, EZMB_DEFAULT_SOUTH_URL);
    lua_setfield(L, -2, "default_south_url");

    lua_pushstring(L, EZMB_DEFAULT_NORTH_URL);
    lua_setfield(L, -2, "default_north_url");

    lua_pushinteger(L, E_DEVICE_TYPE_COLLECTOR);
    lua_setfield(L, -2, "type_collector");

    lua_pushinteger(L, E_DEVICE_TYPE_MONITOR);
    lua_setfield(L, -2, "type_monitor");

    return 1;
}
