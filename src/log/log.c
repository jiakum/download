
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <inttypes.h>
#include <sys/socket.h>
#include "timer.h"
#include "log.h"

static inline int fill_header(char *ptr, int level)
{
    int ms = (get_current_time() & 0x0FFFFFFFF);
    int len = 0;

    ptr[len++] = '$';
    ptr[len++] = '#';
    ptr[len++] = 'L';
    ptr[len++] = level & 0x0f;

    *(int*)(ptr + len) = ms;
    len += sizeof(ms);

    return len;
}

extern int send_log(char *group, char *data, int len);
void rc_log_default_callback(char* group, int level, const char* fmt, va_list vl)
{
    char line[RCLOG_MAX_PACKET_SIZE];
    int len = 0;

    len += fill_header(line + len, level);

    len += vsnprintf(line + len, sizeof(line) - len, fmt, vl);

    send_log(group, line, len);
}

static void (*rc_log_callback)(char*, int, const char*, va_list) = rc_log_default_callback;

void rclog(char *group, int level, const char *fmt, ...)
{
    va_list vl;

    va_start(vl, fmt);

    if(rc_log_callback)
        rc_log_callback(group, level, fmt, vl);

    va_end(vl);
}
