#include "e_device.h"
#include <zmq.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

static void format_device_name(const char *input, char *output, size_t output_size) {
    if (!input) return;

    const char *base = strrchr(input, '/');
    base = base ? base + 1 : input;

    size_t i;
    for (i = 0; i < output_size - 1 && base[i]; ++i) {
        output[i] = tolower((unsigned char)base[i]);
        if (!isalnum((unsigned char)output[i])) {
            output[i] = '_';
        }
    }
    output[i] = '\0';
}


e_device_t *e_device_create(const char *uid, const char *south_url, const char *north_url, e_device_recv_cb cb) {
    if (!uid || !south_url || !north_url) {
        fprintf(stderr, "[ERROR] Invalid arguments to e_device_create\n");
        return NULL;
    }

    e_device_t *device = calloc(1, sizeof(e_device_t));
    if (!device) {
        perror("[ERROR] Failed to allocate e_device_t");
        return NULL;
    }

    device->ctx = zmq_ctx_new();
    if (!device->ctx) {
        fprintf(stderr, "[ERROR] Failed to create ZeroMQ context\n");
        free(device);
        return NULL;
    }

    device->south_sock = zmq_socket(device->ctx, ZMQ_SUB);
    if (!device->south_sock) goto fail;

    device->north_sock = zmq_socket(device->ctx, ZMQ_PUB);
    if (!device->north_sock) goto fail;

    if (zmq_connect(device->south_sock, south_url) != 0 || zmq_connect(device->north_sock, north_url) != 0) {
        fprintf(stderr, "[ERROR] Failed to connect to south/north sockets\n");
        goto fail;
    }

    // 设置在没有连接时丢弃消息
    int immediate = 1;
    zmq_setsockopt(device->north_sock, ZMQ_IMMEDIATE, &immediate, sizeof(immediate));

    // if (south_topic) {
    //     zmq_setsockopt(device->south_sock, ZMQ_SUBSCRIBE, south_topic, strlen(south_topic));
    //     device->south_topic = strdup(south_topic);
    // } else {
        // zmq_setsockopt(device->south_sock, ZMQ_SUBSCRIBE, "", 0);
        // device->south_topic = NULL;
    // }

    char topic_tmp[128] = {0};
    snprintf(topic_tmp, sizeof(topic_tmp), "%s_south_topic", uid);
    device->south_topic = strdup(topic_tmp);
    snprintf(topic_tmp, sizeof(topic_tmp), "%s_north_topic", uid);
    device->north_topic = strdup(topic_tmp);

    zmq_setsockopt(device->south_sock, ZMQ_SUBSCRIBE, device->south_topic, strlen(device->south_topic));

    device->uid = strdup(uid);
    device->south_url = strdup(south_url);
    device->north_url = strdup(north_url);
    device->cb = cb;
    device->running = false;

    return device;

fail:
    if (device->south_sock) zmq_close(device->south_sock);
    if (device->north_sock) zmq_close(device->north_sock);
    if (device->ctx) zmq_ctx_destroy(device->ctx);
    free(device);
    return NULL;
}

void *e_device_listen_thread(void *arg) {
    e_device_t *device = (e_device_t *)arg;
    if (!device || !device->cb) {
        fprintf(stderr, "[ERROR] Invalid device or callback\n");
        return NULL;
    }

    device->running = true;
    zmq_pollitem_t items[] = {
        { device->south_sock, 0, ZMQ_POLLIN, 0 },
    };

    printf("[INFO] Client listener started\n");
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

            const char *topic = (const char *)zmq_msg_data(&topic_msg);
            size_t topic_size = zmq_msg_size(&topic_msg);
            const char *payload = (const char *)zmq_msg_data(&data_msg);
            size_t payload_size = zmq_msg_size(&data_msg);

            device->cb(topic, topic_size, payload, payload_size, device);

            zmq_msg_close(&topic_msg);
            zmq_msg_close(&data_msg);
        }
    }
    printf("[INFO] Client listener stopped\n");
    return NULL;
}

void e_device_listen(e_device_t *device) {
    pthread_t tid;
    pthread_create(&tid, NULL, e_device_listen_thread, device);
    pthread_detach(tid);
}

int e_device_send(e_device_t *device, const char *msg, size_t size) {
    if (!device || !msg || size == 0) return -1;
    int rc = 0;
    rc = zmq_send(device->north_sock, device->north_topic, strlen(device->north_topic), ZMQ_SNDMORE);
    if(rc < 0) return -1;
    rc = zmq_send(device->north_sock, msg, size, 0);
    if(rc < 0) return -1;
    return rc;
}

void e_device_stop(e_device_t *device) {
    if (device) {
        device->running = false;
    }
}

void e_device_destroy(e_device_t *device) {
    if (!device) return;

    e_device_stop(device);
    zmq_close(device->south_sock);
    zmq_close(device->north_sock);
    zmq_ctx_destroy(device->ctx);

    free((char *)device->uid);
    free((char *)device->south_url);
    free((char *)device->north_url);
    if (device->south_topic)
        free((char *)device->south_topic);
    if(device->north_topic)
        free((char *)device->north_topic);


    free(device);
}
