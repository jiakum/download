#ifndef _PACKET_H__
#define _PACKET_H__

#include "protocol.h"

struct io_context;
struct DwPacketContext {
    char *buf, *pos, *start, *end;

    struct DwPacket *dp;
    struct DwPacket *txdp;
    int  len;
    struct io_context *ioc;
};

struct DwPacketContext *dw_init_packet(struct io_context *ioc);
void dw_free_packet(struct DwPacketContext *dpctx);

int dw_get_packet(struct DwPacketContext *context, struct DwPacket **packet);

int dw_write_command_data(struct DwPacketContext *ctx, int cmd, char *data, int len);

#define dw_write_command(ctx, cmd) dw_write_command_data(ctx, cmd, NULL, 0)

#endif
