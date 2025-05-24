#include <stdio.h>
#include <unistd.h>
#include "ezmb.h"

void on_proxy_forward(const char *topic, size_t topic_len, const char *payload, size_t payload_len) {
    printf("[PROXY FORWARDED] Topic: %.*s | Payload: %.*s\n",
           (int)topic_len, topic, (int)payload_len, payload);
}

int main() {
    e_proxy_t *proxy = e_proxy_create(EZMB_DEFAULT_PROXY_FRONTEND_URL, EZMB_DEFAULT_PROXY_BACKEND_URL, on_proxy_forward);
    e_proxy_listen(proxy);
    while (1) {
        sleep(1);
    }
    e_proxy_destroy(proxy);
    return 0;
}
