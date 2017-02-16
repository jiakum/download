#ifndef _PACKET_H__
#define _PACKET_H__

struct DwPacketContext {
    char *buf, *pos, *start, *end;

    struct DwPacket *dp;
    struct DwPacket *txdp;
    int  len;
    struct io_context *ioc;
};


struct DwPacketContext *dw_init_packet(struct io_context *ioc);
int dw_get_packet(struct DwPacketContext *context, struct DwPacket **packet);
int dw_write_command(struct DwPacketContext *ctx, int cmd, char *data, int len);

static inline void dw_next_packet(struct DwPacketContext *ctx)
{
    ctx->len = 0;
}

#endif
