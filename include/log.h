#ifndef _RC_LOG_H__
#define _RC_LOG_H__

#define RCLOG_EMERG  0
#define RCLOG_ALERT  1
#define RCLOG_CRIT   2
#define RCLOG_ERROR  3
#define RCLOG_WARN   4
#define RCLOG_NOTICE 5
#define RCLOG_INFO   6
#define RCLOG_DEBUG  7

#define RCLOG_DEFAULT_LEVEL RCLOG_INFO

#define RCLOG_MAX_PACKET_SIZE (1024 + 512)
#define RCLOGD_SOCKET "/tmp/rclogd"

void rclog(char *group, int level, const char *fmt, ...);
int init_local_logger(char *program);

#endif
