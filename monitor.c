#include "e_device.h"
#include "ezmb.h"
#include <stdio.h>
#include <unistd.h>

static void recv_callback(const char *topic, size_t topic_len, const char *payload, size_t payload_len, void *data) {
    printf("[%s] Received %zu bytes, %.*s\n", topic, payload_len, (int)payload_len, payload);
    e_device_t *monitor = (e_device_t *)data;
    e_monitor_send(monitor, payload, payload_len);
}

int main(int argc, char *argv[]) {
    e_device_t *monitor = e_monitor_create_default("port1", recv_callback);
    e_monitor_listen(monitor);
    while(1) {
        sleep(1);
    }
    e_monitor_destroy(monitor);
    return 0;
}
