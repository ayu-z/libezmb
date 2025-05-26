#include "e_tcp_server.h"
#include "e_tcp_server_config.h"
#include <ezmb/ezmb.h>
#include <ezmb/e_queue.h>
#include <ezmb/e_plugin_driver.h>
#include <stdlib.h>
#include <stdio.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/event_struct.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

static tcp_server_config_t g_config;
static e_queue_t g_queue;
static e_device_t *g_monitor = NULL;
static e_plugin_context_t *g_plugin_ctx = NULL;
static e_tcp_server_t *g_tcpser = NULL;

typedef enum {
    MESSAGE_TYPE_TO_SERVER = 0,
    MESSAGE_TYPE_TO_MONITOR = 1,
} message_type_t;

typedef struct {
    message_type_t type;
    void *payload;
    size_t size;
} message_queue_t;

static void hexdump(const char *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (i % 8 == 0) {
            printf("%04zX: ", i);
        }
        printf("%02X ", (unsigned char)data[i]);
        if (i % 8 == 7 || i == len - 1) {
            if (i % 8 != 7) {
                for (size_t j = 0; j < (7 - (i % 8)); j++) {
                    printf("   ");
                }
            }
            printf(" | ");
            size_t line_start = (i / 8) * 8;
            size_t line_end = line_start + 8;
            for (size_t j = line_start; j < line_end; j++) {
                if (j < len) {
                    unsigned char c = data[j];
                    printf("%c", (c >= 32 && c <= 126) ? c : '.');
                } else {
                    printf(" ");
                }
            }
            printf("\n");
        }
    }
}

static void tcp_server_recv_callback(const void *payload, size_t size, void *data) {
    struct bufferevent *bev = (struct bufferevent *)data;
    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    char ipstr[INET6_ADDRSTRLEN] = {0};
    int port = 0;

    int fd = bufferevent_getfd(bev);
    if (fd >= 0 && getpeername(fd, (struct sockaddr *)&addr, &len) == 0) {
        if (addr.ss_family == AF_INET) {
            struct sockaddr_in *s = (struct sockaddr_in *)&addr;
            inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof(ipstr));
            port = ntohs(s->sin_port);
        } else if (addr.ss_family == AF_INET6) {
            struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
            inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof(ipstr));
            port = ntohs(s->sin6_port);
        }
    }

    printf("[%s:%d] tcp server recv callback:\n", ipstr, port);

    hexdump(payload, size);
    message_queue_t *msg = (message_queue_t *)malloc(sizeof(message_queue_t));
    msg->type = MESSAGE_TYPE_TO_MONITOR;
    msg->payload = malloc(size);
    memcpy(msg->payload, payload, size);
    msg->size = size;
    e_queue_push(&g_queue, msg);
}

static void monitor_recv_callback(const char *topic, size_t topic_len, const  void *payload, size_t payload_len, void *data) {
    (void)data;
    printf("[%.*s]monitor recv callback: \n", (int)topic_len, topic);
    hexdump(payload, payload_len);
    message_queue_t *msg = (message_queue_t *)malloc(sizeof(message_queue_t));
    msg->type = MESSAGE_TYPE_TO_SERVER;
    msg->payload = malloc(payload_len);
    memcpy(msg->payload, payload, payload_len);
    msg->size = payload_len;
    e_queue_push(&g_queue, msg);
}

static void *app_main_thread(void *arg) {
    (void)arg;
    e_plugin_driver_t *driver = (e_plugin_driver_t *)malloc(sizeof(e_plugin_driver_t));
    if (g_config.plugin_enable) {
        driver = e_plugin_load_driver(g_plugin_ctx);
    }
    while (1) {
        if (e_queue_size(&g_queue) > 0) {
            message_queue_t *msg = (message_queue_t *)e_queue_pop(&g_queue);
            printf("app_main_thread: %s, size: %zu, payload: \n", msg->type == MESSAGE_TYPE_TO_SERVER ? "to server" : "to monitor", msg->size);
            hexdump(msg->payload, msg->size);
            switch (msg->type) {
                case MESSAGE_TYPE_TO_SERVER:
                    if (driver->north_transform) {
                        void *out = NULL;
                        size_t out_size = 0;
                        driver->north_transform(g_plugin_ctx, msg->payload, msg->size, (void **)&out, &out_size);
                        printf("north transform done, size: %zu, payload: \n", out_size);
                        hexdump(out, out_size);
                        e_tcp_server_broadcast(g_tcpser, out, out_size);
                        free(out);
                    }else{
                        e_tcp_server_broadcast(g_tcpser, msg->payload, msg->size);
                    }
                    break;
                case MESSAGE_TYPE_TO_MONITOR:
                    if (driver->south_transform) {
                        void *out = NULL;
                        size_t out_size = 0;
                        driver->south_transform(g_plugin_ctx, msg->payload, msg->size, (void **)&out, &out_size);
                        printf("south transform done, size: %zu, payload: \n", out_size);
                        hexdump(out, out_size);
                        e_monitor_send(g_monitor, out, out_size);
                        free(out);
                    }else{
                        e_monitor_send(g_monitor, msg->payload, msg->size);
                    }
                    break;
            }
            free(msg->payload);
            free(msg);
        }else{
            usleep(10000);
        }
    }
    if (driver) {
        free(driver);
    }
    return NULL;
}


int main(int argc, char **argv) {
    if (!e_tcp_server_config_parse(argc, argv, &g_config)) {
        return 1;
    }

    struct event_base *base = event_base_new();
    if (!base) {
        fprintf(stderr, "Could not initialize libevent!\n");
        return 1;
    }

    e_queue_init(&g_queue, 0);

    g_tcpser = e_tcp_server_create(base, g_config.port, tcp_server_recv_callback);
    if (!g_tcpser) {
        fprintf(stderr, "Could not create server!\n");
        event_base_free(base);
        return 1;
    }

    if (g_config.plugin_enable) {
        g_plugin_ctx = e_plugin_create(g_config.plugin_path);
        if (!g_plugin_ctx) {
            printf("Failed to load Lua plugin\n");
            return 1;
        }
    }

    g_monitor = e_monitor_create_default(g_config.uid, monitor_recv_callback);
    if (!g_monitor) {
        fprintf(stderr, "Could not create monitor!\n");
        event_base_free(base);
        return 1;
    }
    e_monitor_listen(g_monitor);

    pthread_t thread;
    pthread_create(&thread, NULL, app_main_thread, NULL);
    pthread_detach(thread);

    event_base_dispatch(base);

    evconnlistener_free(g_tcpser->listener);
    free(g_tcpser->clients);
    free(g_tcpser);
    event_base_free(base);
    e_monitor_destroy(g_monitor);

    printf("done\n");
    return 0;
}