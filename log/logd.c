
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <signal.h>
#include <errno.h>

#include "log.h"
#include "file.h"
#include "list.h"
#include "hash.h"
#include "common.h"

#define LOGD_HTABLE_SIZE_INBITS (7)
#define LOGD_HTABLE_SIZE (1 << LOGD_HTABLE_SIZE_INBITS)
#define LOGD_HTABLE_MASK (LOGD_HTABLE_SIZE - 1)
struct log_server {
    struct hlist_head *heads;
    int fd;
    char *path;

    struct hlist_head uhead;
};

struct log_client {
    struct log_file *file;
    struct sockaddr_un from;
    struct hlist_node list;
};

static inline uint32_t hashfn(char *p, char *g)
{
    uint32_t val = (p[0]) | (p[1] << 8) | (g[0] << 16) | (g[1] << 24);
    return hash_32(val, LOGD_HTABLE_SIZE_INBITS);
}

static void close_client(struct log_client *client)
{
    hlist_del(&client->list);
    if(client->file && client->file->close)
        client->file->close(client->file);

    free(client);
}

static int parse_addr(struct sockaddr_un *from, char *program, char *group)
{
    char *ptr = from->sun_path;

    if(strlen(from->sun_path) < strlen("/tmp/logd_client/"))
        return -1;
    ptr += strlen("/tmp/logd_client/");

    while(*ptr != '/') {
        if(!*ptr)
            return -1;
        *program++ = *ptr++;
    }
    *program = '\0';

    ptr++;
    while(*ptr) {
        *group++ = *ptr++;
    }
    *group = '\0';

    return 0;
}

static int parse_packet(struct log_server *server, struct sockaddr_un *from, unsigned char *buf, int len)
{
    static const char *levelstr[] = {"EMERG", "ALERT", "CRITICAL", "ERROR", "WARNING", "NOTICE", "INFO", "DEBUG"};
    struct log_client *client;
    struct hlist_head *head;
    struct hlist_node *n, *pos;
    char program[64], group[64];
    int ret, key, found = 0;

    ret = parse_addr(from, program, group);
    if(ret < 0 || buf[0] != '$' || buf[1] != '#') {
        printf("Illegal name:%s!!!\n", from->sun_path);
        return -ENOMEM;
    }

    key = hashfn(program, group);
    head = &server->heads[key];
    hlist_for_each_entry(client, n, head, list) {
        if(!strncmp(client->from.sun_path, from->sun_path, sizeof(from->sun_path))) {
            found = 1;
            break;
        }
    }

    if(!found) {
        printf("get packet from:%s, pro:%s, group:%s\n", from->sun_path, program, group);

        client = malloc(sizeof(*client));
        if(!client) {
            printf("%s, no memory!\n", __func__);
            return -ENOMEM;
        }

        memcpy(&client->from, from, sizeof(client->from));

        if(buf[2] == 'L') {
            client->file = open_logfile(program, group);
            if(!client->file) {
                printf("open new log file:%s failed!", from->sun_path);
                free(client);
                return -1;
            }
            hlist_add_head(&client->list, head);
        } else {
            client->file = NULL;
            hlist_add_head(&client->list, &server->uhead);
        }
    }

    if(buf[2] == 'L') {
        struct log_client *uc;
        char str[RCLOG_MAX_PACKET_SIZE + 64];
        int ms = *(int*)(buf + 4), l, level;
        
        level = buf[3] >= ARRAY_SIZE(levelstr) ? ARRAY_SIZE(levelstr) : buf[3];

        buf += 8;
        len -= 8;
        l = snprintf(str, sizeof(str),"[%d.%d][%s][%s][%s]", ms / 1000, ms % 1000, program, group, levelstr[level]);
        memcpy(str + l, buf, len);

        buf = (unsigned char *)str;
        len += l;
        hlist_for_each_entry_safe(uc, pos, n, &server->uhead, list) {
            int left;

            left = len;
            while(left > 0) {
                ret = sendto(server->fd, buf + len - left, left, 0, 
                      (const struct sockaddr *)&uc->from, sizeof(struct sockaddr_un));

                if(ret < 0) {
                    if(errno == EINTR)
                        continue;
                    else if(errno == EAGAIN)
                        break;
                   close_client(uc);
                   break;
                } 

                left -= ret;
            }
        }

        if(client->file->write(client->file, (char*)str, len) < 0)
            close_client(client);

    } else {
        /* need update */
    }

    return 0;
}

static int exit_logd(struct log_server *server)
{
    close(server->fd);
    printf("%s\n", __func__);
    exit(0);
}

static int serverfn(struct epoll_event *event, void *data)
{
    struct log_server *server = data;
    int ret;

    if(event->events & EPOLLIN) {
        unsigned char buf[RCLOG_MAX_PACKET_SIZE];
        struct sockaddr_un addr;
        int len = sizeof(addr);
        
        while(1) {
            ret = recvfrom(server->fd, buf, sizeof(buf), 0, (struct sockaddr*)&addr, (socklen_t *)&len);
            if(ret < 0) {
                if(errno != EAGAIN && errno != EINTR) {
                    printf("%s, recvfrom return error:%s!!!!\n", __func__, strerror(errno));
                    exit_logd(server);
                }
                break;
            } else if(ret == 0) {
                printf("%s, server socket closed ? error:%s. This should never happen!!!!\n", __func__, strerror(errno));
                return -1;
            }

            parse_packet(server, &addr, buf, ret);
        }
    } else if(event->events & (EPOLLHUP | EPOLLERR)) {
        printf("%s, server socket closed ? error:%s. This should never happen!!!!\n", __func__, strerror(errno));
        exit_logd(server);
    }


    return 0;
}

static int epoll_add_server(int epfd, struct log_server *server)
{
    struct epoll_event ev;
    struct epoll_context *epctx = malloc(sizeof(*epctx));

    if(!epctx) {
        printf("%s,malloc failed!\n", __func__);
        return -ENOMEM;
    }

    epctx->callback = serverfn;
    epctx->data     = server;
    ev.events = EPOLLIN;
    ev.data.ptr = epctx;

    if(epoll_ctl(epfd, EPOLL_CTL_ADD, server->fd, &ev) < 0) {
        printf("epoll ctl add failed!\n");
        return -1;
    }

    return 0;
}

static int event_loop(struct log_server *server) 
{
    int epfd, i, ret;

    epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        printf("epoll create failed. err: %s\n", strerror(errno));
        return -1;
    }

    if(epoll_add_server(epfd, server) < 0) {
        printf("%s, epoll add server failed!!!\n", __func__);
        return -1;
    }

    while(1) {
        struct epoll_event events[8];
        do {
            ret = epoll_wait(epfd, events, sizeof(events)/sizeof(events[0]), 100);
            if (ret < 0 && errno != EAGAIN && errno != EINTR) {
                printf("epoll return error:%d! This should never happen!!!\n",ret);
                return -1;
            }
        } while (ret < 0);

        for ( i = 0; i < ret; i++ ) {
            struct epoll_context *epctx = events[i].data.ptr; 

            if(epctx && epctx->callback)
                epctx->callback(&events[i], epctx->data);
            else
                printf("Illegal data! %p,%p\n", epctx, epctx ? epctx->callback : NULL);
        }
    }

    return 0;
}

static int start_server(char *logpath)
{
    struct log_server server;
    struct sockaddr_un addr;
    int fd, i;

    server.heads = malloc(sizeof(struct hlist_head) * LOGD_HTABLE_SIZE);
    if(!server.heads) {
        printf("%s malloc failed!!\n", __func__);
        return -ENOMEM;
    }
    for(i = 0;i < LOGD_HTABLE_SIZE;i++) {
        INIT_HLIST_HEAD(&server.heads[i]);
    }
    INIT_HLIST_HEAD(&server.uhead);

    fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if(fd < 0) {
        printf("create local socket failed,errno:%d\n", errno);
        return -1;
    }

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, RCLOGD_SOCKET, sizeof(addr.sun_path));
    unlink(addr.sun_path);
    if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("bind local socket failed\n");
        close(fd);
        return -1;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    server.fd = fd;

    server.path = strdup(logpath);
    if(!server.path || init_logfile(logpath) < 0) {
        printf("%s,init log file:%s failed!!\n", __func__, logpath);
        return -1;
    }

    return event_loop(&server);
}

static void usage(void)
{



}

int main(int argc, char *argv[])
{
    char *path = "/mnt/data/.logd/", *task = "getlog";
    int i;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    for(i = 1; i < argc; i++) {
        if(!strcmp(argv[i], "-s")) {
            task = "start_server";
        } else if(!strcmp(argv[i], "-d")) {
            if(++i < argc) {
                if(!(path = strdup(argv[i]))) {
                    printf("%s strdup failed!!\n", __func__);
                    return -1;
                }
            } else
                usage();
        }
    }

    if(!strncmp(task, "start_server", strlen("start_server")))
        start_server(path);
    else if(!strncmp(task, "getlog", strlen("getlog"))) {
    }

    return 0;
}
