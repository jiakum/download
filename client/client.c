
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>

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

static int connected_server(struct DwClient *client)
{
    struct DwPacketContext *dpctx;
    struct epoll_event ev;
    int fd = client->fd;

    dpctx = dw_init_packet(fd_to_io_context(fd, IO_TYPE_SOCKET));
    if(!dpctx) {
        printf("%s . No memory!!!\n", __func__);
        close(fd);
        return -ENOMEM;
    }

    client->epctx->callback = handle_data;
    client->epctx->data = client;
    ev.events = EPOLLIN;
    ev.data.ptr = client->epctx;
    if(epoll_ctl(client->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        printf("%s,epoll ctl failed:%s!\n", __func__, strerror(errno));
        dw_free_packet(dpctx);
        return -ENOMEM;
    }

    client->dpctx = dpctx;

    return 0;
}

static int connecting_server(struct epoll_event *ev, void *data)
{
    struct DwClient *client = data;
    int ret;

    if(ev->events & EPOLLOUT) {
        /* socket connect request finished */
        socklen_t optlen = sizeof(ret);
        getsockopt (client->fd, SOL_SOCKET, SO_ERROR, &ret, &optlen);
        if(ret == 0)
            return connected_server(client);
        else
            goto retry;
    }

    if(ev->events & (EPOLLHUP | EPOLLERR)) {
        goto retry;
    }

    return 0;

retry:
    if(epoll_ctl(client->epfd, EPOLL_CTL_DEL, client->fd, NULL) < 0) {
        printf("%s,epoll ctl failed:%s!\n", __func__, strerror(errno));
    }

    close(client->fd);
    update_timer(client->timer, 200);
    return 0;
}

static int try_connect_server(struct timer *timer)
{
    struct DwClient *client = timer->p;
    struct sockaddr_in addr;
    struct epoll_event ev;
    int fd, ret;

    printf("%s\n", __func__);
    fd = socket(AF_INET, SOCK_STREAM  | SOCK_CLOEXEC, 0);
    if(fd < 0) {
        printf("create socket failed:%s!\n", strerror(errno));
        return -ENOMEM;
    }

    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(1212);
    do {
        ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    } while(ret < 0 && errno == EINTR);

    client->fd = fd;
    if(ret < 0 && (errno == EINPROGRESS || errno == EAGAIN)) {
        client->epctx->callback = connecting_server;
        client->epctx->data = client;
        ev.events = EPOLLOUT;
        ev.data.ptr = client->epctx;
        if(epoll_ctl(client->epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
            printf("%s,epoll ctl failed:%s!\n", __func__, strerror(errno));
            return -ENOMEM;
        }
    } else if(ret == 0) {
        return connected_server(client);
    }

    return 0;
}

int init_client(int epfd, struct epoll_context *epctx)
{
    struct DwClient *client = malloc(sizeof(*client));

    if(!epctx || !client)
        return -ENOMEM;
    
    memset(client, 0, sizeof(*client));
    client->epfd = epfd;
    client->epctx = epctx;
    client->timer = add_timer(client, try_connect_server, 60 * 1000);

    return 0;
}

