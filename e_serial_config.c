#include "e_serial_config.h"
#include "e_queue.h"
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>

void e_serial_config_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  -u, --uid <uid>                 UID for this serial device\n");
    fprintf(stderr, "  -D, --device <dev>              Serial device (e.g., /dev/ttyUSB0)\n");
    fprintf(stderr, "  -b, --baud <rate>               Baud rate (default %ld)\n", DEFAULT_SERIAL_BAUDRATE);
    fprintf(stderr, "  -d, --databits <bits>           Data bits (5, 6, 7, 8)\n");
    fprintf(stderr, "  -s, --stopbits <bits>           Stop bits (1 or 2)\n");
    fprintf(stderr, "  -p, --parity <N|E|O>            Parity (None, Even, Odd)\n");
    fprintf(stderr, "  -m, --mindelay <ms>             Minimum delay between sends (ms)\n");
    fprintf(stderr, "  -l, --maxlen <length>           Max frame length (default %d)\n", DEFAULT_BUF_MAX);
    fprintf(stderr, "  -t, --timeout <ms>              Frame timeout in milliseconds (default %d ms)\n", DEFAULT_SERIAL_REV_TIMEOUT);
    fprintf(stderr, "  -C, --config <config.json>      JSON config file for multiple serial ports\n");
    fprintf(stderr, "\nExample:\n");
    fprintf(stderr, "  %s -u ttyusb2 -D /dev/ttyUSB2\n", prog);
    fprintf(stderr, "  %s -C config.json\n", prog);
}

void e_serial_config_print(const serial_config_t *config) {
    printf("\nSerial Configuration:\n");
    printf("  UID           : %s\n", config->uid);
    printf("    Device        : %s\n", config->device);
    printf("    Baudrate      : %d\n", config->baudrate);
    printf("    Data bits     : %d\n", config->databits);
    printf("    Stop bits     : %d\n", config->stopbits);
    printf("    Parity        : %c\n", toupper(config->parity));
    printf("    Min delay (ms): %d\n", config->min_delay_ms);
    printf("    Max length    : %d\n", config->maxlen);
    printf("    Timeout (ms)  : %d\n\n", config->timeout_ms);
}

static void e_serial_config_init(serial_config_t *config) {
    config->uid = NULL;
    config->device = NULL;
    config->baudrate = DEFAULT_SERIAL_BAUDRATE;
    config->databits = 8;
    config->stopbits = 1;
    config->parity = 'n';
    config->min_delay_ms = 0;
    config->maxlen = DEFAULT_BUF_MAX;
    config->timeout_ms = DEFAULT_SERIAL_REV_TIMEOUT;
}

static serial_config_t* e_serial_config_copy(const serial_config_t *src) {
    serial_config_t *dst = malloc(sizeof(serial_config_t));
    if (!dst) return NULL;

    dst->uid = src->uid ? strdup(src->uid) : NULL;
    dst->device = src->device ? strdup(src->device) : NULL;
    dst->baudrate = src->baudrate;
    dst->databits = src->databits;
    dst->stopbits = src->stopbits;
    dst->parity = src->parity;
    dst->min_delay_ms = src->min_delay_ms;
    dst->maxlen = src->maxlen;
    dst->timeout_ms = src->timeout_ms;

    return dst;
}

static void e_serial_config_free(serial_config_t *config) {
    if (config) {
        free(config->uid);
        free(config->device);
        free(config);
    }
}

static int e_serial_config_load_json(const char *filename, e_queue_t *queue) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Failed to open config file");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    rewind(fp);

    char *json_str = malloc(len + 1);
    if (!json_str) {
        fclose(fp);
        return -1;
    }

    fread(json_str, 1, len, fp);
    json_str[len] = '\0';
    fclose(fp);

    struct json_object *root = json_tokener_parse(json_str);
    free(json_str);
    if (!root) {
        fprintf(stderr, "Invalid JSON format\n");
        return -1;
    }

    struct json_object *ports;
    if (!json_object_object_get_ex(root, "ports", &ports) || 
        json_object_get_type(ports) != json_type_array) {
        fprintf(stderr, "Missing or invalid 'ports' array\n");
        json_object_put(root);
        return -1;
    }

    int array_len = json_object_array_length(ports);
    for (int i = 0; i < array_len; i++) {
        struct json_object *item = json_object_array_get_idx(ports, i);
        serial_config_t *config = malloc(sizeof(serial_config_t));
        if (!config) continue;

        e_serial_config_init(config);

        struct json_object *j_uid, *j_device, *j_baud, *j_databits;
        struct json_object *j_stopbits, *j_parity, *j_mindelay;
        struct json_object *j_maxlen, *j_timeout;

        if (json_object_object_get_ex(item, "uid", &j_uid))
            config->uid = strdup(json_object_get_string(j_uid));
        
        if (json_object_object_get_ex(item, "device", &j_device))
            config->device = strdup(json_object_get_string(j_device));

        if (json_object_object_get_ex(item, "baud", &j_baud))
            config->baudrate = json_object_get_int(j_baud);

        if (json_object_object_get_ex(item, "databits", &j_databits))
            config->databits = json_object_get_int(j_databits);
        
        if (json_object_object_get_ex(item, "stopbits", &j_stopbits))
            config->stopbits = json_object_get_int(j_stopbits);

        if (json_object_object_get_ex(item, "parity", &j_parity)) {
            const char *parity = json_object_get_string(j_parity);
            if (parity) config->parity = tolower(parity[0]);
        }

        if (json_object_object_get_ex(item, "mindelay", &j_mindelay))
            config->min_delay_ms = json_object_get_int(j_mindelay);
        
        if (json_object_object_get_ex(item, "maxlen", &j_maxlen))
            config->maxlen = json_object_get_int(j_maxlen);
        
        if (json_object_object_get_ex(item, "timeout", &j_timeout))
            config->timeout_ms = json_object_get_int(j_timeout);

        if (!config->device) {
            e_serial_config_free(config);
            continue;
        }

        e_queue_push(queue, config);
    }

    json_object_put(root);
    return 0;
}

int e_serial_config_parse(int argc, char **argv, e_queue_t *queue) {
    char *config_file = NULL;
    serial_config_t config;
    e_serial_config_init(&config);

    static struct option long_options[] = {
        {"uid", required_argument, 0, 'u'},
        {"device", required_argument, 0, 'D'},
        {"baud", required_argument, 0, 'b'},
        {"databits", required_argument, 0, 'd'},
        {"stopbits", required_argument, 0, 's'},
        {"parity", required_argument, 0, 'p'},
        {"mindelay", required_argument, 0, 'm'},
        {"maxlen", required_argument, 0, 'l'},
        {"timeout", required_argument, 0, 't'},
        {"config", required_argument, 0, 'C'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int long_index = 0;
    while ((opt = getopt_long(argc, argv, "u:D:b:d:s:p:m:l:t:C:h", 
                            long_options, &long_index)) != -1) {
        switch (opt) {
            case 'u':
                config.uid = optarg;
                break;
            case 'D':
                config.device = optarg;
                break;
            case 'b':
                config.baudrate = atoi(optarg);
                break;
            case 'd':
                config.databits = atoi(optarg);
                if (config.databits < 5 || config.databits > 8) {
                    fprintf(stderr, "Invalid data bits (5-8)\n");
                    return -1;
                }
                break;
            case 's':
                config.stopbits = atoi(optarg);
                if (config.stopbits != 1 && config.stopbits != 2) {
                    fprintf(stderr, "Invalid stop bits (1 or 2)\n");
                    return -1;
                }
                break;
            case 'p':
                config.parity = tolower(optarg[0]);
                if (config.parity != 'n' && config.parity != 'e' && 
                    config.parity != 'o') {
                    fprintf(stderr, "Invalid parity (N/E/O)\n");
                    return -1;
                }
                break;
            case 'm':
                config.min_delay_ms = atoi(optarg);
                if (config.min_delay_ms < 0) {
                    fprintf(stderr, "Invalid min delay (>=0)\n");
                    return -1;
                }
                break;
            case 'l':
                config.maxlen = atoi(optarg);
                if (config.maxlen <= 0) {
                    fprintf(stderr, "Invalid max length (>0)\n");
                    return -1;
                }
                break;
            case 't':
                config.timeout_ms = atoi(optarg);
                if (config.timeout_ms <= 0) {
                    fprintf(stderr, "Invalid timeout (>0)\n");
                    return -1;
                }
                break;
            case 'C':
                config_file = strdup(optarg);   
                break;
            case 'h':
                e_serial_config_usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                return -1;
        }
    }

    if (config_file) {
        if (e_serial_config_load_json(config_file, queue) != 0) {
            fprintf(stderr, "Failed to load config file\n");
            return -1;
        }
    } 
    else if (config.device && config.uid) {
        serial_config_t *cfg_copy = e_serial_config_copy(&config);
        if (cfg_copy) {
            e_queue_push(queue, cfg_copy);
        }
    } else {
        e_serial_config_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    return 0;
}

#if 0
int e_serial_config_load_json_callback(void *arg, void *data) {
    serial_config_t *config = (serial_config_t *)data;
    e_serial_config_print(config);
    return 0;
}

int main(int argc, char **argv) {
    e_queue_t port_queue;
    e_queue_init(&port_queue, 0);
    
    char *config_file = NULL;
    
    if (e_serial_config_parse(argc, argv, &port_queue) != 0) {
        fprintf(stderr, "Parameter parsing failed\n");
        e_queue_clear(&port_queue);
        free(config_file);
        return EXIT_FAILURE;
    }

    printf("\nLoaded %d port configurations:\n", e_queue_size(&port_queue));
    e_queue_foreach(&port_queue, NULL, e_serial_config_load_json_callback);

    e_queue_clear(&port_queue);
    free(config_file);
    
    return EXIT_SUCCESS;
}
#endif