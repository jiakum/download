
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
#include "utils.h"
#include "list.h"

struct server_client {
    int fd;
    struct sockaddr_in addr;

    struct epoll_context epctx;
    struct DwPacketContext *dpctx;

    struct list_head list;
};

struct DwServer {
    int epfd, fd;
    struct epoll_context *epctx;

    struct list_head clients;
};
static struct DwServer *dwserver;

static void disconnect_client(struct server_client *client)
{
    struct DwServer *server = dwserver;

    epoll_ctl(server->epfd, EPOLL_CTL_DEL, client->fd, NULL);
    dw_free_packet(client->dpctx);
    
    list_del(&client->list);
    free(client);
}

static int proc_cmd(struct server_client *client, struct DwPacket *dp)
{
    int cmd = dp->cmd;

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

static int handle_client(struct epoll_event *ev, void *data)
{
    struct server_client *client = data;
    struct DwPacketContext *dpctx = client->dpctx;
    struct DwPacket *dp;
    int ret;

    if(ev->events & EPOLLIN) {
        do {
            ret = dw_get_packet(dpctx, &dp);

            if(ret < 0) {
                disconnect_client(client);
                return 0;
            } else if(ret == 0)
                break;
            else
                proc_cmd(client, dp);

        } while(1);
    }

    if(ev->events & (EPOLLHUP | EPOLLERR)) {
        disconnect_client(client);
        return 0;
    }

    return 0;
}

static int new_client(struct DwServer *server)
{
    struct server_client *client = malloc(sizeof(*client));
    struct DwPacketContext *dpctx;
    struct epoll_event ev;
    int len = sizeof(struct sockaddr_in), fd;

    if(!client) { 
        printf("%s,no memory!!!\n", __func__);
        return -ENOMEM;
    }

    if((fd = accept(server->fd, (struct sockaddr *)&client->addr, (socklen_t *)&len)) < 0) {
        printf("accept failed:%s!!!\n", strerror(errno));
        return -1;
    }

    dpctx = dw_init_packet(fd_to_io_context(fd, IO_TYPE_SOCKET));
    if(!dpctx) { 
        printf("%s, no memory!!!\n", __func__);
        close(fd);
        return -ENOMEM;
    }

    list_add(&client->list, &server->clients);
    client->dpctx = dpctx;
    client->fd = fd;

    client->epctx.callback = handle_client;
    client->epctx.data = client;
    ev.events = EPOLLIN;
    ev.data.ptr = &client->epctx;

    if(epoll_ctl(server->epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        printf("epoll ctl failed:%s!\n", strerror(errno));
        disconnect_client(client);
        return -1;
    }

#if 0
    dw_write_command(client->dpctx, RCCOMMAND_REQUEST_NEWFILE);
    dw_write_command(client->dpctx, RCCOMMAND_REQUEST_NEWFILE);
    dw_write_command(client->dpctx, RCCOMMAND_REQUEST_NEWFILE);
    dw_write_command(client->dpctx, RCCOMMAND_REQUEST_NEWFILE);
    dw_write_command(client->dpctx, RCCOMMAND_REQUEST_NEWFILE);
#endif

    return 0;
}

static int handle_server(struct epoll_event *ev, void *data)
{
    struct DwServer *server = data;

    if(ev->events & EPOLLIN)
        return new_client(server);

    return 0;
}

int init_server(int epfd, struct epoll_context *epctx)
{
    struct DwServer *server = malloc(sizeof(*server));
    struct sockaddr_in addr;
    struct epoll_event ev;
    int fd;

    if(!epctx || !server)
        return -ENOMEM;
    
    memset(server, 0, sizeof(*server));
    INIT_LIST_HEAD(&server->clients);
    dwserver = server;
    
    server->epfd = epfd;
    server->epctx = epctx;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(1212);
    if((fd = socket_open_listen(&addr)) < 0) {
        printf("connect remote failed:%s!\n", strerror(errno));
        return -1;
    }

    epctx->callback = handle_server;
    epctx->data = server;
    ev.events = EPOLLIN;
    ev.data.ptr = server->epctx;
    
    if(epoll_ctl(server->epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        printf("%s,epoll ctl failed:%s!\n", __func__, strerror(errno));
        close(fd);
        return -1;
    }

    server->fd = fd;

    return 0;
}

