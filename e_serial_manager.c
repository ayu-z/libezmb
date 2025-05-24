#include "e_serial_manager.h"
#include "e_queue.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <poll.h>
#include <sys/stat.h>



static int find_port_index(e_queue_t *q, const char *uid) {
    int index = 0;
    e_qnode_t *node = q->top->next;
    while (node) {
        serial_context_t *ctx = (serial_context_t *)node->data;
        if (ctx && strcmp(ctx->ser.uid, uid) == 0) {
            return index;
        }
        node = node->next;
        index++;
    }
    return -1;
}


static void *read_thread_func(void *arg) {
    serial_manager_t *manager = (serial_manager_t *)arg;
    
    while (manager->running) {
        pthread_mutex_lock(&manager->mutex);
        
        // 动态分配pollfd数组
        struct pollfd *pfds = malloc(sizeof(struct pollfd) * manager->ports.size);
        if (!pfds) {
            pthread_mutex_unlock(&manager->mutex);
            usleep(100000);
            continue;
        }
        
        int valid_fds = 0;
        e_qnode_t *node = manager->ports.top->next;
        
        // 收集有效的文件描述符
        while (node) {
            serial_context_t *ctx = (serial_context_t *)node->data;
            if (ctx && ctx->running) {
                pthread_mutex_lock(&ctx->fd_mutex);
                if (ctx->fd >= 0) {
                    pfds[valid_fds].fd = ctx->fd;
                    pfds[valid_fds].events = POLLIN | POLLERR | POLLHUP;
                    pfds[valid_fds].revents = 0;
                    valid_fds++;
                }
                pthread_mutex_unlock(&ctx->fd_mutex);
            }
            node = node->next;
        }
        
        pthread_mutex_unlock(&manager->mutex);
        
        if (valid_fds == 0) {
            free(pfds);
            usleep(100000);
            continue;
        }
        
        // 监控文件描述符
        int ret = poll(pfds, valid_fds, 100);
        if (ret < 0) {
            free(pfds);
            if (errno == EINTR) continue;
            perror("poll error");
            continue;
        }
        
        if (ret == 0) {
            free(pfds);
            continue;
        }
        
        // 处理事件
        pthread_mutex_lock(&manager->mutex);
        for (int i = 0; i < valid_fds; i++) {
            if (pfds[i].revents == 0) continue;
            
            // 查找对应上下文
            serial_context_t *target_ctx = NULL;
            e_qnode_t *node = manager->ports.top->next;
            while (node) {
                serial_context_t *ctx = (serial_context_t *)node->data;
                if (ctx && ctx->fd == pfds[i].fd) {
                    target_ctx = ctx;
                    break;
                }
                node = node->next;
            }
            
            if (!target_ctx || !target_ctx->running) continue;
            
            // 处理错误事件
            if (pfds[i].revents & (POLLERR | POLLHUP)) {
                fprintf(stderr, "[%s] Port error/hangup\n", target_ctx->ser.uid);
                pthread_mutex_lock(&target_ctx->fd_mutex);
                close(target_ctx->fd);
                target_ctx->fd = -1;
                pthread_mutex_unlock(&target_ctx->fd_mutex);
                continue;
            }
            
            // 处理数据到达
            if (pfds[i].revents & POLLIN) {
                pthread_mutex_lock(&target_ctx->fd_mutex);
                char *buf = malloc(target_ctx->ser.maxlen);
                if (!buf) {
                    pthread_mutex_unlock(&target_ctx->fd_mutex);
                    continue;
                }
                
                ssize_t n = read(target_ctx->fd, buf, target_ctx->ser.maxlen);
                if (n <= 0) {
                    free(buf);
                    if (n < 0 && errno != EAGAIN) {
                        perror("read error");
                        close(target_ctx->fd);
                        target_ctx->fd = -1;
                    }
                    pthread_mutex_unlock(&target_ctx->fd_mutex);
                    continue;
                }
                
                if (target_ctx->recv_cb) {
                    target_ctx->recv_cb(target_ctx, buf, n);
                }
                free(buf);
                pthread_mutex_unlock(&target_ctx->fd_mutex);
            }
        }
        pthread_mutex_unlock(&manager->mutex);
        free(pfds);
    }
    return NULL;
}

static void *monitor_thread_func(void *arg) {
    serial_manager_t *manager = (serial_manager_t *)arg;
    
    while (manager->running) {
        sleep(1); // 1秒检测间隔
        
        pthread_mutex_lock(&manager->mutex);
        e_qnode_t *node = manager->ports.top->next;
        while (node) {
            serial_context_t *ctx = (serial_context_t *)node->data;
            if (ctx && ctx->running) {
                pthread_mutex_lock(&ctx->fd_mutex);
                if (ctx->fd < 0 && access(ctx->ser.device, F_OK) == 0) {
                    ctx->fd = serial_open(&ctx->ser);
                    if (ctx->fd >= 0) {
                        printf("[Monitor] %s reopened\n", ctx->ser.uid);
                    }
                }
                pthread_mutex_unlock(&ctx->fd_mutex);
            }
            node = node->next;
        }
        pthread_mutex_unlock(&manager->mutex);
    }
    return NULL;
}


serial_manager_t *e_serial_manager_create(void) {
    serial_manager_t *manager = calloc(1, sizeof(serial_manager_t));
    if (!manager) return NULL;
    
    e_queue_init(&manager->ports, 0);
    pthread_mutex_init(&manager->mutex, NULL);
    return manager;
}

void e_serial_manager_destroy(serial_manager_t *manager) {
    if (!manager) return;
    
    e_serial_manager_stop(manager);
    
    pthread_mutex_lock(&manager->mutex);
    while (!e_queue_empty(&manager->ports)) {
        serial_context_t *ctx = e_queue_pop(&manager->ports);
        if (ctx) {
            ctx->running = 0;
            pthread_mutex_lock(&ctx->fd_mutex);
            if (ctx->fd >= 0) close(ctx->fd);
            pthread_mutex_unlock(&ctx->fd_mutex);
            pthread_mutex_destroy(&ctx->fd_mutex);
            free(ctx);
        }
    }
    e_queue_destroy(&manager->ports);
    pthread_mutex_unlock(&manager->mutex);
    
    pthread_mutex_destroy(&manager->mutex);
    free(manager);
}

int e_serial_manager_add_port(serial_manager_t *manager, 
                          const serial_config_t *config,
                          serial_recv_callback_t recv_cb,
                          void *data) {
    if (!manager || !config) return -1;
    
    // 检查UID唯一性
    pthread_mutex_lock(&manager->mutex);
    if (find_port_index(&manager->ports, config->uid) >= 0) {
        pthread_mutex_unlock(&manager->mutex);
        fprintf(stderr, "UID %s exists\n", config->uid);
        return -1;
    }
    
    // 创建新上下文
    serial_context_t *ctx = calloc(1, sizeof(serial_context_t));
    if (!ctx) {
        pthread_mutex_unlock(&manager->mutex);
        return -1;
    }
    
    memcpy(&ctx->ser, config, sizeof(serial_config_t));
    ctx->fd = -1;
    ctx->running = 1;
    ctx->recv_cb = recv_cb;
    pthread_mutex_init(&ctx->fd_mutex, NULL);
    ctx->data = data;
    e_queue_push(&manager->ports, ctx);
    pthread_mutex_unlock(&manager->mutex);
    return 0;
}

void e_serial_manager_remove_port(serial_manager_t *manager, const char *uid) {
    if (!manager || !uid) return;
    
    pthread_mutex_lock(&manager->mutex);
    
    // 创建临时队列
    e_queue_t temp;
    e_queue_init(&temp, 0);
    
    while (!e_queue_empty(&manager->ports)) {
        serial_context_t *ctx = e_queue_pop(&manager->ports);
        if (ctx && strcmp(ctx->ser.uid, uid) == 0) {
            ctx->running = 0;
            pthread_mutex_lock(&ctx->fd_mutex);
            if (ctx->fd >= 0) close(ctx->fd);
            pthread_mutex_unlock(&ctx->fd_mutex);
            pthread_mutex_destroy(&ctx->fd_mutex);
            free(ctx);
        } else {
            e_queue_push(&temp, ctx);
        }
    }
    
    // 恢复剩余端口
    while (!e_queue_empty(&temp)) {
        e_queue_push(&manager->ports, e_queue_pop(&temp));
    }
    e_queue_destroy(&temp);
    
    pthread_mutex_unlock(&manager->mutex);
}

int e_serial_manager_write(serial_manager_t *manager, 
                       const char *uid,
                       const void *data, 
                       size_t len) {
    if (!manager || !uid || !data || len == 0) return -1;
    
    pthread_mutex_lock(&manager->mutex);
    e_qnode_t *node = manager->ports.top->next;
    while (node) {
        serial_context_t *ctx = (serial_context_t *)node->data;
        if (ctx && strcmp(ctx->ser.uid, uid) == 0) {
            pthread_mutex_lock(&ctx->fd_mutex);
            if (ctx->fd < 0) {
                pthread_mutex_unlock(&ctx->fd_mutex);
                pthread_mutex_unlock(&manager->mutex);
                return -1;
            }
            
            ssize_t ret = write(ctx->fd, data, len);
            pthread_mutex_unlock(&ctx->fd_mutex);
            pthread_mutex_unlock(&manager->mutex);
            return ret;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&manager->mutex);
    return -1;
}

void e_serial_manager_start(serial_manager_t *manager) {
    if (!manager || manager->running) return;
    
    manager->running = 1;
    pthread_create(&manager->read_tid, NULL, read_thread_func, manager);
    pthread_create(&manager->monitor_tid, NULL, monitor_thread_func, manager);
}

void e_serial_manager_stop(serial_manager_t *manager) {
    if (!manager || !manager->running) return;
    
    manager->running = 0;
    pthread_join(manager->read_tid, NULL);
    pthread_join(manager->monitor_tid, NULL);
}

int e_serial_manager_size(serial_manager_t *manager) {
    if (!manager) return 0;
    return e_queue_size(&manager->ports);
}