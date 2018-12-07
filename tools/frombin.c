
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#define min(a,b) ((a) < (b) ? (a): (b))
#define LINE_SIZE (32)

static int write_all(int fd, unsigned char *buf, int len)
{
    int left = len;

    while(left > 0) {
        int ret = write(fd, buf, left);
        if(ret < 0) {
            if(errno == EINTR || errno == EAGAIN)
                continue;

            return ret;
        }

        buf  += ret;
        left -= ret;
    }

    return 0;
}

int main(int argc, char **argv)
{
    unsigned char bin_buf[1024], out_buf[LINE_SIZE * 8], *ptr;
    int bin, fd, ret, len, left;

    if(argc < 3) {
        printf("usage: %s binary_filename output.c\n", argv[0]);
        return -EINVAL;
    }

    bin = open(argv[1], O_RDONLY);
    if(bin < 0) {
        printf("usage: %s binary_filename output.c\n", argv[0]);
        return -EINVAL;
    }

    fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0) {
        printf("usage: %s binary_filename output.c\n", argv[0]);
        return -EINVAL;
    }
    
    len = snprintf(out_buf, sizeof(out_buf), "static unsigned char chinese_font[] = {\n");
    if(write_all(fd, out_buf, len) < 0) {
        perror("write file failed");
        return -EIO;
    }

    do {
        ret = read(bin, bin_buf, sizeof(bin_buf));
        if(ret < 0) {
            if(errno == EINTR || errno == EAGAIN)
                continue;

            break;
        } else if(ret == 0) {
            break;
        }

        ptr = bin_buf;
        while(ret > 0) {
            int i, size = min(ret, LINE_SIZE);
            len = 0;
            for(i = 0;i < size;i++) {
                len += snprintf(out_buf + len, sizeof(out_buf) - len, "0x%02x,", ptr[i]);
            }
            len += snprintf(out_buf + len, sizeof(out_buf) - len, "\n");

            if(write_all(fd, out_buf, len) < 0) {
                perror("write file failed");
                return -EIO;
            }

            ret -= size;
            ptr += size;
        }

    } while(1);

    if(lseek(fd, -2, SEEK_CUR) < 0) {
        perror("seek file failed");
        return -EIO;
    }

    len = snprintf(out_buf, sizeof(out_buf), "\n};\n");
    if(write_all(fd, out_buf, len) < 0) {
        perror("write file failed");
        return -EIO;
    }

    close(bin);
    close(fd);

    return 0;
}
