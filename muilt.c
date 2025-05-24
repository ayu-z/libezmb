#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <poll.h>
#include <zmq.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>
#include <json-c/json.h>

#define DEFAULT_SERIAL_BAUDRATE 115200
#define DEFAULT_SERIAL_REV_TIMEOUT 100
#define DEFAULT_BUF_MAX 1024

typedef struct {
    char device[128];
    int baudrate;
    int databits;
    int stopbits;
    char parity;
    int mindelay;
    int maxlen;
    int timeout;
    char topic[128];
    void *zmq_pub;
} serial_conf_t;

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  -D, --device <dev>              Serial device (e.g., /dev/ttyUSB0)\n");
    fprintf(stderr, "  -b, --baud <rate>               Baud rate (default %d)\n", DEFAULT_SERIAL_BAUDRATE);
    fprintf(stderr, "  -d, --databits <bits>           Data bits (5, 6, 7, 8)\n");
    fprintf(stderr, "  -s, --stopbits <bits>           Stop bits (1 or 2)\n");
    fprintf(stderr, "  -p, --parity <N|E|O>            Parity (None, Even, Odd)\n");
    fprintf(stderr, "  -m, --min-delay <ms>            Minimum delay between sends (ms)\n");
    fprintf(stderr, "  -l, --maxlen <length>           Max frame length (default %d)\n", DEFAULT_BUF_MAX);
    fprintf(stderr, "  -t, --timeout <ms>              Frame timeout in milliseconds (default %d ms)\n", DEFAULT_SERIAL_REV_TIMEOUT);
    fprintf(stderr, "  -T, --topic <topic>             Topic for this serial device\n");
    fprintf(stderr, "  -C, --config <config.json>      JSON config file for multiple serial ports\n");
    fprintf(stderr, "\nExample:\n");
    fprintf(stderr, "  %s -D /dev/ttyUSB2 -b 115200 -d 8 -s 1 -p N -T port1\n", prog);
}

int open_serial(const char *device, int baudrate, int databits, int stopbits, char parity) {
    int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return -1;

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    tcgetattr(fd, &tty);

    cfsetospeed(&tty, baudrate);
    cfsetispeed(&tty, baudrate);

    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= (databits == 7) ? CS7 : CS8;

    if (parity == 'E')
        tty.c_cflag |= PARENB;
    else if (parity == 'O') {
        tty.c_cflag |= (PARENB | PARODD);
    } else {
        tty.c_cflag &= ~PARENB;
    }

    tty.c_cflag &= ~CSTOPB;
    if (stopbits == 2)
        tty.c_cflag |= CSTOPB;

    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_iflag = tty.c_oflag = tty.c_lflag = 0;

    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &tty);

    return fd;
}

void *serial_thread(void *arg) {
    serial_conf_t *conf = (serial_conf_t *)arg;


    printf("conf->device: %s\n", conf->device);
    printf("conf->baudrate: %d\n", conf->baudrate);
    printf("conf->databits: %d\n", conf->databits);
    printf("conf->stopbits: %d\n", conf->stopbits);
    printf("conf->parity: %c\n", conf->parity);
    printf("conf->mindelay: %d\n", conf->mindelay);
    printf("conf->maxlen: %d\n", conf->maxlen);
    printf("conf->timeout: %d\n", conf->timeout);
    printf("conf->topic: %s\n", conf->topic);

    
    int fd = open_serial(conf->device, conf->baudrate, conf->databits, conf->stopbits, conf->parity);
    if (fd < 0) {
        perror("Failed to open serial");
        return NULL;
    }

    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    uint8_t buf[DEFAULT_BUF_MAX];

    while (1) {
        if (poll(&pfd, 1, conf->timeout) > 0) {
            int len = read(fd, buf, conf->maxlen);
            if (len > 0) {
                zmq_send(conf->zmq_pub, conf->topic, strlen(conf->topic), ZMQ_SNDMORE);
                zmq_send(conf->zmq_pub, buf, len, 0);
            }
        }
        usleep(conf->mindelay * 1000);
    }
    close(fd);
    return NULL;
}

void load_json_config(const char *filename, void *zmq_pub) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("fopen");
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = 0;
    fclose(f);

    struct json_object *root = json_tokener_parse(buf);
    free(buf);

    // ⚠️ 新增：从 "ports" 字段提取数组
    struct json_object *ports_array = NULL;
    if (!json_object_object_get_ex(root, "ports", &ports_array) ||
        json_object_get_type(ports_array) != json_type_array) {
        fprintf(stderr, "Invalid config file: expected \"ports\" array\n");
        json_object_put(root);
        exit(1);
    }

    int count = json_object_array_length(ports_array);
    for (int i = 0; i < count; i++) {
        struct json_object *item = json_object_array_get_idx(ports_array, i);
        serial_conf_t *conf = calloc(1, sizeof(serial_conf_t));

        // 读取必须字段
        const char *device = json_object_get_string(json_object_object_get(item, "device"));
        if (!device) continue;
        strncpy(conf->device, device, sizeof(conf->device));

        struct json_object *baud_obj = json_object_object_get(item, "baudrate");
        if (!baud_obj) continue;
        conf->baudrate = json_object_get_int(baud_obj);

        // 可选字段处理（添加默认值）
        conf->databits = json_object_get_int(json_object_object_get(item, "databits") ?: json_object_new_int(8));
        conf->stopbits = json_object_get_int(json_object_object_get(item, "stopbits") ?: json_object_new_int(1));
        const char *parity = json_object_get_string(json_object_object_get(item, "parity") ?: json_object_new_string("N"));
        conf->parity = parity ? parity[0] : 'N';

        conf->mindelay = json_object_get_int(json_object_object_get(item, "mindelay") ?: json_object_new_int(0));
        conf->maxlen = json_object_get_int(json_object_object_get(item, "maxlen") ?: json_object_new_int(DEFAULT_BUF_MAX));
        conf->timeout = json_object_get_int(json_object_object_get(item, "timeout") ?: json_object_new_int(DEFAULT_SERIAL_REV_TIMEOUT));

        const char *topic = json_object_get_string(json_object_object_get(item, "topic"));
        snprintf(conf->topic, sizeof(conf->topic), "%s", topic ? topic : conf->device);

        conf->zmq_pub = zmq_pub;

        pthread_t tid;
        pthread_create(&tid, NULL, serial_thread, conf);
        pthread_detach(tid);
    }

    json_object_put(root);
}


int main(int argc, char *argv[]) {
    serial_conf_t conf = {
        .baudrate = DEFAULT_SERIAL_BAUDRATE,
        .databits = 8,
        .stopbits = 1,
        .parity = 'N',
        .mindelay = 0,
        .maxlen = DEFAULT_BUF_MAX,
        .timeout = DEFAULT_SERIAL_REV_TIMEOUT
    };
    char config_file[128] = "";

    static struct option long_options[] = {
        {"device",  required_argument, 0, 'D'},
        {"baud",    required_argument, 0, 'b'},
        {"databits",required_argument, 0, 'd'},
        {"stopbits",required_argument, 0, 's'},
        {"parity",  required_argument, 0, 'p'},
        {"min-delay",required_argument,0, 'm'},
        {"maxlen",  required_argument, 0, 'l'},
        {"timeout", required_argument, 0, 't'},
        {"topic",   required_argument, 0, 'T'},
        {"config",  required_argument, 0, 'C'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "D:b:d:s:p:m:l:t:T:C:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'D': strncpy(conf.device, optarg, sizeof(conf.device)); break;
            case 'b': conf.baudrate = atoi(optarg); break;
            case 'd': conf.databits = atoi(optarg); break;
            case 's': conf.stopbits = atoi(optarg); break;
            case 'p': conf.parity = optarg[0]; break;
            case 'm': conf.mindelay = atoi(optarg); break;
            case 'l': conf.maxlen = atoi(optarg); break;
            case 't': conf.timeout = atoi(optarg); break;
            case 'T': strncpy(conf.topic, optarg, sizeof(conf.topic)); break;
            case 'C': strncpy(config_file, optarg, sizeof(config_file)); break;
            default: print_usage(argv[0]); exit(EXIT_FAILURE);
        }
    }

    void *ctx = zmq_ctx_new();
    void *pub = zmq_socket(ctx, ZMQ_PUB);
    zmq_bind(pub, "ipc:///tmp/serial.ipc");

    if (strlen(config_file)) {
        load_json_config(config_file, pub);
    } else if (strlen(conf.device) && strlen(conf.topic)) {
        conf.zmq_pub = pub;
        pthread_t tid;
        pthread_create(&tid, NULL, serial_thread, &conf);
        pthread_detach(tid);
        while (1) pause();
    } else {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    while (1) pause();
    return 0;
}
