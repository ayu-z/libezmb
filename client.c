#include "ezmb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define FRONTEND_URL EZMB_DEFAULT_NORTH_URL
#define BACKEND_URL  EZMB_DEFAULT_SOUTH_URL
#define TOPIC        "demo"

void on_client_recv(const char *topic, size_t topic_len, const char *payload, size_t payload_len, void *data) {
    printf("[CLIENT RECEIVED] Topic: %.*s | Payload: %.*s\n",
           (int)topic_len, topic, (int)payload_len, payload);
}

void *device_listen_thread(void *arg) {
    e_device_listen((e_device_t *)arg);
    return NULL;
}

int main() {
    e_device_t *device = e_collector_create("demo", EZMB_DEFAULT_SOUTH_URL, EZMB_DEFAULT_NORTH_URL, on_client_recv);

    e_device_listen(device);

    printf("[SENDER] Sending message...\n");
    int i = 0;
    char msg[100];
    while (1) {
        sprintf(msg, "Hello from client! %d", i++);
        printf("[SENDER] Sending message: %s\n", msg);
        e_collector_send(device, msg, strlen(msg));
        sleep(5);
    }

    e_device_destroy(device);

    return 0;
}
