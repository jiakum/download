#ifndef _IO_H__
#define _IO_H__

enum IO_TYPE {
    IO_TYPE_SOCKET = 0x01,
    IO_TYPE_FILE,
};

/* read write func
 * ret < 0 for error
 * ret = 0 for no packet read
 * ret > 0 for read success, ret is bytes read
 * */

struct io_context {
    int fd;

    int (*read)(struct io_context *io, char *buf, int len);
    int (*write)(struct io_context *io, char *buf, int len);
};
struct io_context *fd_to_io_context(int fd, int io_type);

#endif
