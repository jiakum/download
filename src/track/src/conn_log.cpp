
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include <sys/mman.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#include "conn_log.h"
#include "sql.h"

using namespace std;

#define USE_SQL_PARSER
    
#define ASSERT_BUFSIZE(buf, size, end)  do { \
                                                if(buf == NULL || buf + size >= end) \
                                                    return -1; \
                                            } while(0)

#ifndef USE_SQL_PARSER
static int parse_record(struct conn_log *clog, char *buf, char *end)
{
    char *start = buf, target[512];

    while(buf < end && *buf != '(') buf++;
    buf++;

    ASSERT_BUFSIZE(buf, 16, end);
    buf += parse_one(buf, end, target, sizeof(target));
    snprintf(clog->uid, sizeof(clog->uid), "%s", target);

    ASSERT_BUFSIZE(buf, 16, end);
    buf = record_skip_n(buf, end, 9);

    ASSERT_BUFSIZE(buf, 16, end);
    buf += parse_one(buf, end, target, sizeof(target));
    clog->type = strtol(buf, NULL, 0);

    return buf - start;
}
#else
static struct sql_record record;
static int parse_record(struct conn_log *clog, char *buf, char *end)
{
    char *start = buf;
    if(sql_read_one_record(&buf, end, &record) < 0)
        return -1;

    assert(record.elist[0].type == ELEMENT_CHAR 
        || record.elist[0].type == ELEMENT_UCHAR
        || record.elist[0].type == ELEMENT_VARCHAR);
    snprintf(clog->uid, sizeof(clog->uid), "%s", record.elist[0].data.ptr);
    clog->type = record.elist[11].data.bigint;

    return buf - start;
}
#endif

int parse_conn_log(const char *filename, unordered_map<string, int> &uidmap,
        std::unordered_map<std::string, struct conn_status> &result)
{
    char *start = (char *)MAP_FAILED, *buf, *end;
    std::unordered_map<std::string, struct conn_status>::iterator iter;
    int fd, filesize;
        
    if((fd = open(filename, O_RDONLY)) < 0)
        goto failed;
    
    filesize = lseek(fd, 0, SEEK_END);
    if(filesize < 0)
        goto failed;
    lseek(fd, 0, SEEK_SET);
    
    start = (char *)mmap(NULL, filesize, PROT_READ, MAP_SHARED, fd, 0);
    if(start == MAP_FAILED)
        goto failed;
    
    end = start + filesize;
    buf = strnstr(start, end, "INSERT INTO ");
    if(buf)
        buf = strnstr(start, end, "VALUES ");
    if(buf == NULL)
        goto failed;

#ifdef USE_SQL_PARSER
    if(sql_read_talbe_title(start, end, &record) < 0
        || sql_preinit_record(&record, 1024) < 0) {
        goto failed;
    }
#endif

    while(buf < end) {
        struct conn_log clog;

        int ret = parse_record(&clog, buf, end);
        if(ret < 0)
            break;

        buf += ret;

        if(uidmap.find(string(clog.uid)) == uidmap.end())
            continue;

        iter = result.find(string(clog.uid));
        
        if(iter == result.end()) {
            struct conn_status status;

            memset(&status, 0, sizeof(status));
            if(clog.type < 0 || clog.type > 3) {
                status.failed++;
            } else {
                status.success[clog.type] = 1;
            }

            result[string(clog.uid)] = status;
        } else {
            struct conn_status &status = iter->second;
            
            if(clog.type < 0 || clog.type > 3) {
                status.failed++;
            } else {
                status.success[clog.type]++;
            }
        }

    }
    
    munmap(start, filesize);
    close(fd);
    
    return 0;

failed:
    printf("parse conn log:%s failed:%s\n", filename, strerror(errno));
    if(start != MAP_FAILED)
        munmap(start, filesize);
    if(fd >= 0)
        close(fd);

    return -1;
}
