#include "e_serial_manager.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

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
    printf("[%s] Received %zu bytes:\n", port->ser.uid, len);
    hexdump(data, len);
}

int main() {
    serial_manager_t *manager = e_serial_manager_create();
    if (!manager) {
        fprintf(stderr, "Failed to create serial manager\n");
        return 1;
    }

    serial_config_t config1 = {
        .uid = "UART1",
        .device = "/tmp/virtual_serial1",
        .baudrate = 115200,
        .databits = 8,
        .parity = 'N',
        .stopbits = 1,
        .timeout_ms = 100,
        .maxlen = 1024
    };

    serial_config_t config2 = {
        .uid = "UART2",
        .device = "/tmp/virtual_serial2",
        .baudrate = 9600,
        .databits = 8,
        .parity = 'N',
        .stopbits = 1,
        .timeout_ms = 100,
        .maxlen = 1024
    };


    if (e_serial_manager_add_port(manager, &config1, recv_callback, NULL) != 0 ||
        e_serial_manager_add_port(manager, &config2, recv_callback, NULL) != 0) {
        fprintf(stderr, "Failed to add serial ports\n");
        e_serial_manager_destroy(manager);
        return 1;
    }

    printf("serial size: %d\n", e_serial_manager_size(manager));

    e_serial_manager_start(manager);

    while (1) {

        sleep(1);

        const char *test_data = "Hello Serial Port!";
        e_serial_manager_write(manager, "UART1", test_data, strlen(test_data));
        char test_data2[32] = {0};
        for (int i = 0; i < 32; i++) {
            test_data2[i] = 0x31+i;
        }
        e_serial_manager_write(manager, "UART2", test_data2, sizeof(test_data2));
    }

    e_serial_manager_stop(manager);
    e_serial_manager_destroy(manager);
    return 0;
}