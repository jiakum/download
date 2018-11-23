
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <time.h>

#include "device_type.h"
#include "conn_log.h"
#include "output.h"

using namespace std;

int main(int argc, char **argv)
{
    char filename[128];

    if(argc < 2) {
        struct tm calendar;
        time_t curt = time(NULL) - 24 * 3600;

        if(gmtime_r(&curt, &calendar) == NULL) {
            printf("get current time failed\n");
            return -1;
        }

        snprintf(filename, sizeof(filename), "%04d-%02d-%02d.sql", calendar.tm_year + 1900,
                                            calendar.tm_mon + 1, calendar.tm_mday);
        printf("default use yesterday:%s\n", filename);
    } else {
        snprintf(filename, sizeof(filename), "%s.sql", argv[1]);
    }

    unordered_map<string, int> lmap(10 * 1000);
    if(parse_device_type(lmap, "device_type_lib.json", TRACK_DEVICE_TYPE_GO) < 0) {
        printf("parse device type error!!\n");
        return -1;
    }
    printf("parse device type success, uid count:%ld\n", lmap.size());
    
    unordered_map<string, struct conn_status> result(10 * 1000);
    if(parse_conn_log(filename, lmap, result) < 0)
        return -1;

    output_conn_status_csv(result, NULL);

    return 0;
}
