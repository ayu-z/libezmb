#include "e_serial_manager.h"
#include "ezmb.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>

#define MAX_THREAD_COUNT 10 // 最大线程数(暂定，后期修改使用线程池)

static e_queue_t g_device_queue;
static serial_manager_t *g_manager;

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

static void recv_callback(void *ctx, const char *data, size_t len) {
    serial_context_t *port = (serial_context_t *)ctx;
    e_device_t *device = (e_device_t *)port->data;

    printf("[%s] Received %zu bytes:\n", port->ser.uid, len);
    hexdump(data, len);
    printf("send to north topic: %s, rc = %d\n", device->north_topic, e_device_send(device, data, len));
}

static void on_client_recv(const char *topic, size_t topic_len, const char *payload, size_t payload_len, void *data) {
    e_device_t *device = (e_device_t *)data;
    printf("[CLIENT RECEIVED] uid: %s | Topic: %.*s | Payload: %.*s\n",
           device->uid, (int)topic_len, topic, (int)payload_len, payload);

    e_serial_manager_write(g_manager, device->uid, payload, payload_len);
    
}

static void *device_listen_thread(void *arg) {
    e_device_t *device = (e_device_t *)arg;
    printf("[DEVICE LISTEN THREAD] Start, device: %s, south_topic: %s, north_topic: %s\n", device->uid, device->south_topic, device->north_topic);
    e_device_listen(device);
    printf("[DEVICE LISTEN THREAD] End, device: %s\n", device->uid);
    return NULL;
}

int main(int argc, char **argv) {

    e_queue_t port_queue;
    e_queue_init(&port_queue, 0);

    if (e_config_parse_args(argc, argv, &port_queue) != 0) {
        fprintf(stderr, "Failed to parse arguments\n");
        exit(EXIT_FAILURE);
    }

    if(e_queue_size(&port_queue) > MAX_THREAD_COUNT) {
        fprintf(stderr, "Too many serial ports, max is %d\n", MAX_THREAD_COUNT);
        exit(EXIT_FAILURE);
    }

    g_manager = e_serial_manager_create();
    if (!g_manager) {
        fprintf(stderr, "Failed to create serial manager\n");
        return 1;
    }
    e_queue_init(&g_device_queue, 0);

    while (e_queue_size(&port_queue) > 0) {
        serial_config_t *config = (serial_config_t *)e_queue_pop(&port_queue);
        e_device_t *device = e_device_create(config->uid, EZMB_DEFAULT_SOUTH_URL, EZMB_DEFAULT_NORTH_URL, on_client_recv);
        
        if (e_serial_manager_add_port(g_manager, config, recv_callback, device) != 0) {
            fprintf(stderr, "Failed to add serial port %s\n", config->uid);
            e_serial_manager_destroy(g_manager);
            exit(EXIT_FAILURE);
        }
        
        e_queue_push(&g_device_queue, device);
    }

    pthread_t thread[MAX_THREAD_COUNT];
    int thread_count = 0;
    while (e_queue_size(&g_device_queue) > 0) {
        e_device_t *device = (e_device_t *)e_queue_pop(&g_device_queue);
        pthread_create(&thread[thread_count++], NULL, device_listen_thread, device);
    }

    e_queue_destroy(&port_queue);

    e_serial_manager_start(g_manager);

    while (1) {
        sleep(1);
    }

    e_serial_manager_stop(g_manager);
    e_serial_manager_destroy(g_manager);
    return 0;
}