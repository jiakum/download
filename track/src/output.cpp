
#include "output.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

using namespace std;

static int write_all(int fd, char *buf, int size)
{
    int left = size;

    while(left > 0) {
        int ret = write(fd, buf, left);

        if(ret < 0) {
            if(errno == EINTR || errno == EAGAIN)
                continue;
            return -1;
        }

        left -= ret;
        buf += ret;
    }

    return 0;
}

int output_conn_status_csv(unordered_map<string, struct conn_status> &result
        , const char *filename)
{
    unordered_map<string, struct conn_status>::iterator iter;
    char buf[1024], default_name[64];
    unsigned long success[3] = {0, 0, 0}, fails = 0;
    int fd, len;
    
    if(filename == NULL) {        
        struct tm calendar;
        time_t curt = time(NULL) - 24 * 3600;

        if(gmtime_r(&curt, &calendar) == NULL) {
            printf("get current time failed\n");
            return -1;
        }

        snprintf(default_name, sizeof(default_name), "conn_status_%04d-%02d-%02d.csv", calendar.tm_year + 1900,
                                            calendar.tm_mon + 1, calendar.tm_mday);
        filename = default_name;
        printf("default output file:%s\n", filename);
    }

    fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0666);
    if(fd < 0)
        goto failed;

    len = snprintf(buf, sizeof(buf), "uid,local,p2p,relay,success,fails,percent\n");
    if(write_all(fd, buf, len) < 0)
        goto failed;

    for(iter = result.begin();iter != result.end();iter++) {
        struct conn_status &status = iter->second;
        int ss = status.success[0] + status.success[1]+ status.success[2];
        len = snprintf(buf, sizeof(buf), "%s,%d,%d,%d,%d,%d,%f\n"
            , iter->first.c_str()
            , status.success[0], status.success[1], status.success[2]
            , ss, status.failed, (float)ss / (ss + status.failed));
        if(write_all(fd, buf, len) < 0)
            goto failed;


        success[0] += status.success[0];
        success[1] += status.success[1];
        success[2] += status.success[2];
        fails += status.failed;
    }
    
    len = snprintf(buf, sizeof(buf), "%s,%ld,%ld,%ld,%ld,%ld,%f\n"
        , "total"
        , success[0], success[1], success[2]
        , success[0] + success[1] + success[2], fails
        , (float)(success[0] + success[1] + success[2]) / (success[0] + success[1] + success[2] + fails));
    if(write_all(fd, buf, len) < 0)
        goto failed;

    close(fd);

    return 0;
failed:
    printf("write output file:%s failed:%s\n", filename, strerror(errno));

    if(fd >= 0)
        close(fd);

    return -1;
}
