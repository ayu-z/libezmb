#include "e_serialport.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <getopt.h>


static speed_t get_baud_constant(int baudrate) {
    switch (baudrate) {
        case 50: return B50;
        case 75: return B75;
        case 110: return B110;
        case 134: return B134;
        case 150: return B150;
        case 200: return B200;
        case 300: return B300;
        case 600: return B600;
        case 1200: return B1200;
        case 1800: return B1800;
        case 2400: return B2400;
        case 4800: return B4800;
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 500000: return B500000;
        case 576000: return B576000;
        case 921600: return B921600;
        case 1000000: return B1000000;
        case 1152000: return B1152000;
        case 1500000: return B1500000;
        case 2000000: return B2000000;
        case 2500000: return B2500000;
        case 3000000: return B3000000;
        case 3500000: return B3500000;
        case 4000000: return B4000000;
        default:
            fprintf(stderr, "Unsupported baudrate %d\n", baudrate);
            return B0;
    }
}


int serial_configure(int fd, const serial_config_t *cfg) {
    struct termios options;
    if (tcgetattr(fd, &options) < 0) {
        perror("tcgetattr failed");
        return -1;
    }

    cfmakeraw(&options);

    // 设置波特率
    speed_t baud = get_baud_constant(cfg->baudrate);
    if (baud == B0) {
        return -1;
    }
    cfsetispeed(&options, baud);
    cfsetospeed(&options, baud);

    // 设置数据位
    options.c_cflag &= ~CSIZE;
    switch (cfg->databits) {
        case 5: options.c_cflag |= CS5; break;
        case 6: options.c_cflag |= CS6; break;
        case 7: options.c_cflag |= CS7; break;
        case 8: options.c_cflag |= CS8; break;
        default: return -1;
    }

    // 设置奇偶校验
    switch (cfg->parity) {
        case 'n': case 'N':
            options.c_cflag &= ~PARENB;
            break;
        case 'o': case 'O':
            options.c_cflag |= PARENB;
            options.c_cflag |= PARODD;
            break;
        case 'e': case 'E':
            options.c_cflag |= PARENB;
            options.c_cflag &= ~PARODD;
            break;
        default: return -1;
    }

    // 停止位
    if (cfg->stopbits == 2) options.c_cflag |= CSTOPB;
    else options.c_cflag &= ~CSTOPB;

    // 启用接收器并设置本地模式
    options.c_cflag |= (CLOCAL | CREAD);
    
    // 禁用硬件流控
    options.c_cflag &= ~CRTSCTS;

    // 设置输入选项
    options.c_iflag &= ~(IXON | IXOFF | IXANY); // 禁用软件流控制
    options.c_iflag &= ~(INLCR | ICRNL); // 禁用NL/CR转换

    // 设置输出选项
    options.c_oflag &= ~OPOST;

    // 设置控制选项
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // 非规范模式

    // 设置超时特性
    options.c_cc[VMIN] = 0;  // 非阻塞读取
    options.c_cc[VTIME] = 0; // 无超时

    if (tcsetattr(fd, TCSANOW, &options) < 0) {
        perror("tcsetattr failed");
        return -1;
    }

    tcflush(fd, TCIOFLUSH);

    return 0;
}

int serial_open(const serial_config_t *cfg) {
    int fd = open(cfg->device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        perror("open_serial failed");
        return -1;
    }

    if (serial_configure(fd, cfg) < 0) {
        serial_close(&fd);
        return -1;
    }

    return fd;
}

void serial_close(int *fd) {
    if (*fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

size_t serial_write(serial_context_t *ctx, const char *data, size_t len) {
    if (!ctx || ctx->fd < 0) {
        return -1;
    }
    pthread_mutex_lock(&ctx->fd_mutex);
    ssize_t n = write(ctx->fd, data, len);
    pthread_mutex_unlock(&ctx->fd_mutex);
    return n;
}