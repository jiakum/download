
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "protocol.h"
#include "packet.h"
#include "io.h"

struct DwPacketContext *dw_init_packet(struct io_context *ioc)
{
    struct DwPacketContext *context = malloc(sizeof(*context));
    char *buf = malloc(sizeof(struct DwPacket) * 16);
    struct DwPacket *dp = malloc(sizeof(*dp));
    struct DwPacket *txdp = malloc(sizeof(*txdp));

    if(!context || !buf || !dp || !txdp) {
        printf("malloc failed!!!\n");
        return NULL;
    }

    if(!ioc->read || !ioc->write) {
        free(buf); free(dp); free(txdp); free(context);
        printf("%s, need read and write function for io context!\n", __func__);
        return NULL;
    }

    memset(context, 0, sizeof(*context));
    context->buf = context->start = context->pos = buf;
    context->end = context->start + sizeof(struct DwPacket) * 16;
    context->dp = dp;
    context->txdp = txdp;
    context->ioc = ioc;
    printf("%p, %p, %p, %p\n", context, dp, txdp, ioc); fflush(stdout);
    printf("%p, %p, %p, %p\n", context->buf, context->start, context->end, context->pos); fflush(stdout);

    return context;
}

void dw_free_packet(struct DwPacketContext *dpctx)
{
    printf("%p, %p, %p, %p\n", dpctx, dpctx->dp, dpctx->txdp, dpctx->ioc); fflush(stdout);
    printf("%p, %p, %p, %p\n", dpctx->buf, dpctx->start, dpctx->end, dpctx->pos); fflush(stdout);
    dpctx->ioc->close(dpctx->ioc);
    printf("%s, %d\n", __func__, __LINE__); fflush(stdout);
    free(dpctx->start);
    printf("%s, %d\n", __func__, __LINE__); fflush(stdout);
    free(dpctx->dp);
    printf("%s, %d\n", __func__, __LINE__); fflush(stdout);
    free(dpctx->txdp);
    printf("%s, %d\n", __func__, __LINE__); fflush(stdout);
    free(dpctx);
    printf("%s, %d\n", __func__, __LINE__); fflush(stdout);
}

static int read_cached(struct DwPacketContext *ctx, struct io_context *ioc, char *buf, int len)
{
    int ret, left = len, offset;

retry:
    offset = len - left;

    if(ctx->pos - ctx->buf >= left) {
        memcpy(buf + offset, ctx->buf, left);
        ctx->buf += left;

        return len;
    } else if(ctx->pos - ctx->buf > 0) {

        memcpy(buf + offset, ctx->buf, ctx->pos - ctx->buf);
        left -= ctx->pos - ctx->buf;
        ctx->buf = ctx->pos;
    }

    if(ctx->buf == ctx->end - 1)
        ctx->buf = ctx->pos = ctx->start;
    ret = ioc->read(ioc, ctx->pos, ctx->end - ctx->pos);
    if(ret < 0)
        return ret;
    else if(ret == 0)
        return len - left;

    ctx->pos += ret;
    goto retry;
}

int dw_get_packet(struct DwPacketContext *ctx, struct DwPacket **packet)
{
    struct DwPacket *dp = ctx->dp;
    char *buf = (char *)dp;
    int ret, len;

    *packet = NULL;
retry:
    len = ctx->len >= RCPACKET_HEAD_LEN ? (dp->len + RCPACKET_HEAD_LEN - ctx->len): (RCPACKET_HEAD_LEN - ctx->len);
    ret = read_cached(ctx, ctx->ioc, buf + ctx->len, len);
    if(ret < 0)
        return ret;
    else if(ret == 0)
        return 0;

    ctx->len += ret;
    len = ctx->len;

    if(len < RCPACKET_HEAD_LEN)
        return 0;
    else if(len == RCPACKET_HEAD_LEN) {
        char crc;

        if(buf[0] != '@' || buf[1] != '$') {
            int ptr = 0;
            do {
                if(++ptr > len -2) {
                    break;
                }
            } while(buf[ptr] != '@' || buf[ptr + 1] != '$');
            ctx->len = len - ptr;
            memcpy(buf, buf + ptr, len - ptr);
            goto retry;
        }

        crc = calc_crc8((unsigned char *)buf, len - 2);
        if(crc != buf[len -2] || buf[len - 1] != ~crc) {
            printf("%s,crc check failed\n", __func__);
            memcpy(buf, buf + 2, len - 2);
            ctx->len -= 2;
            goto retry;
        }

        if(dp->len == 0)
            goto success;
        else
            goto retry;
    }

    if(len < RCPACKET_HEAD_LEN + dp->len)
        goto retry;

success:
    *packet = dp;
    ctx->len = 0;
    return len;
}

int dw_write_command_data(struct DwPacketContext *ctx, int cmd, char *data, int len)
{
    struct DwPacket *dp = ctx->txdp;

    if(len > (int)sizeof(dp->payload))
        return -1;

    dp->header[0] = '@';
    dp->header[1] = '$';
    dp->flags  = 0x0;
    dp->cmd    = cmd & 0x0ffff;
    dp->len    = len;
    dp->seq++;
    dp->crc[0] = calc_crc8((unsigned char *)dp, RCPACKET_HEAD_LEN - 2);
    dp->crc[1] = ~dp->crc[0];

    if(data && len > 0)
        memcpy(dp->payload, data, len);

    return ctx->ioc->write(ctx->ioc, (char *)dp, RCPACKET_HEAD_LEN + len);
}

