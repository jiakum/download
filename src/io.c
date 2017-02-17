
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "io.h"

static int read_socket(struct io_context *ctx, char *buf, int len)
{
    int ret;

    ret = recv(ctx->fd, buf, len, 0);
    if(ret < 0) {
        if(errno != EAGAIN && errno != EINTR)
            return -1;
        return 0;
    } else if(ret == 0)
        return -1;
    else
        return ret;
}

static int write_socket(struct io_context *ctx, char *buf, int len)
{
    int ret, left = len;

    while(left > 0) {
        ret = send(ctx->fd, buf + len - left, left, 0);
        if(ret < 0) {
            if(errno == EINTR)
                continue;
            printf("send return error:%s\n", strerror(errno));
            return -1;
        } else
            left -= ret;
    }

    return len;
}

static void free_io_context(struct io_context * ioc)
{
    close(ioc->fd);
    free(ioc);
}

struct io_context *fd_to_io_context(int fd, int io_type) 
{
    struct io_context *ctx = malloc(sizeof(*ctx));

    if(!ctx)
        return NULL;

    ctx->fd = fd;
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    if(io_type == IO_TYPE_SOCKET) {
        ctx->read = read_socket;
        ctx->write = write_socket;
    }
    ctx->close = free_io_context;

    return ctx;
}
