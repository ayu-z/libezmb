// #include "e_plugin_driver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

typedef void (*e_plugin_message_transform_callback)(const void *in_data, size_t in_data_size, char **out_data, size_t *out_data_size);

typedef struct e_plugin_driver {
    e_plugin_message_transform_callback south_transform;
    e_plugin_message_transform_callback north_transform;
} e_plugin_driver_t;


static bool is_elf(const char *filename) {
    const char *elf_magic = "\x7f""ELF";  //so库文件的头部
    unsigned char header[4];
    FILE *f = fopen(filename, "rb");
    if (!f) return false;
    if (fread(header, 1, 4, f) != 4) {
        fclose(f);
        return false;
    }
    fclose(f);
    return (memcmp(header, elf_magic, 4) == 0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <plugin_path>\n", argv[0]);
        return 1;
    }

    const char *plugin_path = argv[1];
    if (!is_elf(plugin_path)) {
        printf("Error: %s is not an ELF file\n", plugin_path);
        return 1;
    }

    printf("Loading plugin: %s\n", plugin_path);

    return 0;
}
