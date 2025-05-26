#include "e_tcp_server_config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>


void e_tcp_server_config_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  -u, --uid <uid>                 UID for south device\n");
    fprintf(stderr, "  -l, --listen <port>             TCP server port (e.g., 8080)\n");
    fprintf(stderr, "  -p, --plugin <path>             Plugin path (e.g., /usr/lib/e_plugin.so)\n");
    fprintf(stderr, "\nExample:\n");
    fprintf(stderr, "  %s -u ttyusb2 -l 8080 -p /usr/lib/e_plugin.so\n", prog);
}

void e_tcp_server_config_print(const tcp_server_config_t *config) {
    printf("\nTCP Server Configuration:\n");
    printf("  UID           : %s\n", config->uid);
    printf("    Listen port  : %d\n", config->port);
    printf("    Plugin path  : %s\n", config->plugin_path);
    printf("    Plugin enable: %s\n", config->plugin_enable ? "true" : "false");
}

static void e_tcp_server_config_init(tcp_server_config_t *config) {
    config->uid = NULL;
    config->port = 0;
    config->plugin_path = NULL;
    config->plugin_enable = false;
}

bool e_tcp_server_config_parse(int argc, char **argv, tcp_server_config_t *config) {
    e_tcp_server_config_init(config);

    static struct option long_options[] = {
        {"uid", required_argument, 0, 'u'},
        {"listen", required_argument, 0, 'l'},
        {"plugin", required_argument, 0, 'p'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int long_index = 0;
    while ((opt = getopt_long(argc, argv, "u:l:p:h", 
                            long_options, &long_index)) != -1) {
        switch (opt) {
            case 'u':
                config->uid = strdup(optarg);
                break;
            case 'l':
                config->port = atoi(optarg);
                break;
            case 'p':
                config->plugin_path = strdup(optarg);
                config->plugin_enable = true;
                break;
            case 'h':
                e_tcp_server_config_usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                return false;
        }
    }

    if (config->uid == NULL || config->port == 0) {
        e_tcp_server_config_usage(argv[0]);
        return false;
    }

    return true;
}
