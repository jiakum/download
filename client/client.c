
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "common.h"
#include "timer.h"
#include "packet.h"
#include "io.h"
#include "log.h"

struct DwClient {
    int epfd, fd;
    struct epoll_context *epctx;
    struct timer *timer;
    struct DwPacketContext *dpctx;
};

static void disconnect_server(struct DwClient *client)
{
    epoll_ctl(client->epfd, EPOLL_CTL_DEL, client->fd, NULL);
    dw_free_packet(client->dpctx);

    client->fd = 0;
    client->dpctx = NULL;

    update_timer(client->timer, 200);
}

static int proc_cmd(struct DwClient *client, struct DwPacket *dp)
{
    int cmd = dp->cmd;

    //printf("recieve command:0x%x\n", cmd);
    switch(cmd)
    {
    case RCCOMMAND_REQUEST_NEWFILE:
        break;

    case RCCOMMAND_TRANS_FILEDATA:

        break;

    case RCCOMMAND_FILEDATA_MD5:

        break;

    case RCCOMMAND_END_TRANSFILE:
        break;

    default:
        printf("unkown command:0x%x\n", cmd);
        break;
    }

    return 1;
}

static int handle_data(struct epoll_event *ev, void *data)
{
    struct DwClient *client = data;
    struct DwPacketContext *dpctx = client->dpctx;
    struct DwPacket *dp;
    int ret;

    if(ev->events & EPOLLIN) {
        do {
            ret = dw_get_packet(dpctx, &dp);

            if(ret < 0) {
                disconnect_server(client);
                return 0;
            } else if(ret == 0)
                break;
            else
                proc_cmd(client, dp);

        } while(1);
    }

    if(ev->events & (EPOLLHUP | EPOLLERR)) {
        disconnect_server(client);
        return 0;
    }

    return 0;
}

static int connect_server(struct timer *timer)
{
    struct DwClient *client = timer->p;
    struct DwPacketContext *dpctx;
    struct sockaddr_in addr;
    struct epoll_event ev;
    int fd;

    fd = socket(AF_INET, SOCK_STREAM  | SOCK_CLOEXEC, 0);
    if(fd < 0) {
        printf("create socket failed:%s!\n", strerror(errno));
        goto retry;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(1212);
    if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        int i;
        for(i = 0;i < 1024;i++) {
            rclog("download", RCLOG_INFO, "connect remote failed:%s!\n", strerror(errno));
            rclog("client", RCLOG_INFO, "connect remote failed:%s!\n", strerror(errno));
            rclog("server", RCLOG_INFO, "connect remote failed:%s!\n", strerror(errno));
            rclog(__func__, RCLOG_INFO, "connect remote failed:%s!\n", strerror(errno));
            rclog(__FILE__, RCLOG_INFO, "connect remote failed:%s!\n", strerror(errno));
            rclog("download", RCLOG_INFO, "connect remote failed:%s!\n", strerror(errno));
            rclog("download", RCLOG_INFO, "connect remote failed:%s!\n", strerror(errno));
            rclog("download", RCLOG_INFO, "connect remote failed:%s!\n", strerror(errno));
            rclog("download", RCLOG_INFO, "connect remote failed:%s!\n", strerror(errno));
            rclog("download", RCLOG_INFO, "connect remote failed:%s!\n", strerror(errno));
            rclog("download", RCLOG_INFO, "connect remote failed:%s!\n", strerror(errno));
            rclog("download", RCLOG_INFO, "connect remote failed:%s!\n", strerror(errno));
            rclog("download", RCLOG_INFO, "connect remote failed:%s!\n", strerror(errno));
            rclog("download", RCLOG_INFO, "connect remote failed:%s!\n", strerror(errno));
        }
        close(fd);
        goto retry;
    }

    dpctx = dw_init_packet(fd_to_io_context(fd, IO_TYPE_SOCKET));
    if(!dpctx) {
        printf("%s . No memory!!!\n", __func__);
        close(fd);
        goto retry;
    }

    ev.events = EPOLLIN;
    ev.data.ptr = client->epctx;
    if(epoll_ctl(client->epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        printf("%s,epoll ctl failed:%s!\n", __func__, strerror(errno));
        dw_free_packet(dpctx);
        goto retry;
    }

    client->fd = fd;
    client->dpctx = dpctx;
    stop_timer(timer);

    return 0;

retry:
    update_timer(timer, 200);

    return 0;
}

int init_client(int epfd, struct epoll_context *epctx)
{
    struct DwClient *client = malloc(sizeof(*client));

    if(!epctx || !client)
        return -ENOMEM;
    
    memset(client, 0, sizeof(*client));
    epctx->callback = handle_data;
    epctx->data = client;
    client->epfd = epfd;
    client->epctx = epctx;
    client->timer = add_timer(client, connect_server, 200);

    return 0;
}

