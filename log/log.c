
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

static inline int sprint_prefix(char *ptr, int max, char *group, const char* level)
{
    int64_t ms = get_current_time();
    int len;

    len = snprintf(ptr, max, "[%"PRId64".%d][%s][%s]", ms / 1000, (int)(ms % 1000), group, level);

    return len;
}

extern int send_log(char *group, char *data, int len);
void rc_log_default_callback(char* group, int level, const char* fmt, va_list vl)
{
    static const char *levelstr[] = {"EMERG", "ALERT", "CRITICAL", "ERROR", "WARNING", "NOTICE", "INFO", "DEBUG"};
    char line[RCLOG_MAX_PACKET_SIZE];
    int len;

    if(level >= (int)(sizeof(levelstr)/sizeof(levelstr[0])))
        return;

    line[0] = level;
    len = 1;

    len += sprint_prefix(line + len, sizeof(line) - len, group, levelstr[level]);

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
