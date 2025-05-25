#include "e_plugin_driver.h"
#include <stdio.h>
#include <stdlib.h>
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


void run_plugin(e_plugin_context_t *ctx) {
    if (!ctx) {
        printf("Invalid plugin context\n");
        return;
    }

    const e_plugin_driver_t *driver = e_plugin_load_driver(ctx);
    if (!driver || !driver->north_transform || !driver->south_transform) {
        printf("Failed to get valid driver\n");
        return;
    }

    const char *data = "Hello";
    char *out = NULL;
    size_t out_size = 0;

    const char *label = e_plugin_type(ctx) == PLUGIN_LUA ? "Lua" : "ELF";

    printf("\n-- %s plugin --\n", label);

    driver->north_transform(ctx, data, strlen(data), (void **)&out, &out_size);
    printf("string north returned: size:%d, %.*s\n", (int)out_size, (int)out_size, out);
    free(out);

    driver->south_transform(ctx, data, strlen(data), (void **)&out, &out_size);
    printf("string south returned: size:%d, %.*s\n", (int)out_size, (int)out_size, out);
    free(out);

    const char data2[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    driver->north_transform(ctx, data2, sizeof(data2), (void **)&out, &out_size);
    printf("hex north returned: size:%d, \n", (int)out_size);
    hexdump(out, out_size);
    free(out);

    driver->south_transform(ctx, data2, sizeof(data2), (void **)&out, &out_size);
    printf("hex south returned: size:%d, \n", (int)out_size);
    hexdump(out, out_size);
    free(out);
    
}


int main() {
    e_plugin_context_t *ctx1 = NULL;
    e_plugin_context_t *ctx2 = NULL;

    ctx1 = e_plugin_create("plug.lua");
    if (!ctx1) {
        printf("Failed to load Lua plugin\n");
        return 1;
    }
    ctx2 = e_plugin_create("./plugin_elf.so");
    if (!ctx2) {
        printf("Failed to load ELF plugin\n");
        return 1;
    }

    run_plugin(ctx1);
    run_plugin(ctx2);

    e_plugin_destroy(ctx1);
    e_plugin_destroy(ctx2);
    return 0;
}
