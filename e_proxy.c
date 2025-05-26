#include "e_proxy.h"
#include <zmq.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static int recv_msg(void *socket, zmq_msg_t *msg) {
    zmq_msg_init(msg);
    return zmq_msg_recv(msg, socket, 0);
}

static int send_msg(void *socket, zmq_msg_t *msg, int flags) {
    return zmq_msg_send(msg, socket, flags);
}

e_proxy_t *e_proxy_create(const char *frontend_url, const char *backend_url, e_message_callback callback) {
    if (!frontend_url || !backend_url) {
        fprintf(stderr, "[ERROR] Invalid arguments to e_proxy_create\n");
        return NULL;
    }

    e_proxy_t *proxy = calloc(1, sizeof(e_proxy_t));
    if (!proxy) {
        perror("[ERROR] Failed to allocate e_proxy_t");
        return NULL;
    }

    proxy->ctx = zmq_ctx_new();
    if (!proxy->ctx) {
        fprintf(stderr, "[ERROR] Failed to create ZMQ context\n");
        free(proxy);
        return NULL;
    }

    proxy->frontend = zmq_socket(proxy->ctx, ZMQ_XSUB);
    if (!proxy->frontend) goto fail;

    proxy->backend = zmq_socket(proxy->ctx, ZMQ_XPUB);
    if (!proxy->backend) goto fail;

    if (zmq_bind(proxy->frontend, frontend_url) != 0 ||
        zmq_bind(proxy->backend, backend_url) != 0) {
        fprintf(stderr, "[ERROR] Failed to bind frontend or backend socket\n");
        goto fail;
    }

    proxy->callback = callback;
    proxy->running = false;
    return proxy;

fail:
    if (proxy->frontend) zmq_close(proxy->frontend);
    if (proxy->backend) zmq_close(proxy->backend);
    if (proxy->ctx) zmq_ctx_destroy(proxy->ctx);
    free(proxy);
    return NULL;
}

void e_proxy_destroy(e_proxy_t *proxy) {
    if (!proxy) return;
    e_proxy_stop(proxy);
    zmq_close(proxy->frontend);
    zmq_close(proxy->backend);
    zmq_ctx_destroy(proxy->ctx);
    free(proxy);
}


void *e_proxy_listen_thread(void *arg) {
    e_proxy_t *proxy = (e_proxy_t *)arg;
    if (!proxy) return NULL;

    proxy->running = true;

    zmq_pollitem_t items[] = {
        { proxy->frontend, 0, ZMQ_POLLIN, 0 },
        { proxy->backend,  0, ZMQ_POLLIN, 0 }
    };

    printf("[INFO] Proxy started\n");

    while (proxy->running) {
        int rc = zmq_poll(items, 2, 100);  // 100ms timeout
        if (rc == -1)
            continue;

        // Messages from publishers (frontend) → subscribers (backend)
        if (items[0].revents & ZMQ_POLLIN) {
            zmq_msg_t topic_msg, data_msg;

            if (recv_msg(proxy->frontend, &topic_msg) == -1 ||
                recv_msg(proxy->frontend, &data_msg) == -1) {
                zmq_msg_close(&topic_msg);
                zmq_msg_close(&data_msg);
                continue;
            }

            if (proxy->callback) {
                const char *topic = (const char *)zmq_msg_data(&topic_msg);
                size_t topic_size = zmq_msg_size(&topic_msg);
                const char *payload = (const char *)zmq_msg_data(&data_msg);
                size_t payload_size = zmq_msg_size(&data_msg);

                proxy->callback(topic, topic_size, payload, payload_size);
            }

            send_msg(proxy->backend, &topic_msg, ZMQ_SNDMORE);
            send_msg(proxy->backend, &data_msg, 0);

            zmq_msg_close(&topic_msg);
            zmq_msg_close(&data_msg);
        }

        // Subscription messages from subscribers (backend) → publishers (frontend)
        if (items[1].revents & ZMQ_POLLIN) {
            zmq_msg_t msg;
            if (recv_msg(proxy->backend, &msg) == -1) continue;
            send_msg(proxy->frontend, &msg, 0);
            zmq_msg_close(&msg);
        }
    }

    printf("[INFO] Proxy stopped\n");
    return NULL;
}

void e_proxy_listen(e_proxy_t *proxy) {
    pthread_t tid;
    pthread_create(&tid, NULL, e_proxy_listen_thread, proxy);
    pthread_detach(tid);
}

void e_proxy_stop(e_proxy_t *proxy) {
    if (proxy) {
        proxy->running = false;
    }
}
