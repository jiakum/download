#ifndef _LOG_FILE_H__
#define _LOG_FILE_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "list.h"


#define LOGFILE_MAXSIZE (1024 * 1024)
#define MAX_LOGFILES    (512)
#define MAXLOGS_FOR_ONE (99)
#define LOGFILE_CLEAN_STEP (16)

struct log_file {
    int fd, size;
    char *path, *group, *filename;
    struct hlist_node list;
    int num, index;

    int (*write)(struct log_file *file, char *buf, int len);
    int (*close)(struct log_file *file);
};

struct log_file *open_logfile(char *program, char *group);
int init_logfile(char *path);


static inline int check_dir(char *path)
{
    struct stat pathstat;

    if(access(path, F_OK) < 0) {
        char *ptr = path;
        char name[128];
        int n = 0;

        if(strlen(path) >= sizeof(name))
            return -1;

        while(1) {
            if(*ptr == '/') {
                name[n++] = '/';
                ptr++;
            }

            if(*ptr == '\0')
                break;

            while(*ptr && *ptr != '/')
                name[n++] = *ptr++;

            name[n] = '\0';

            if(access(name, F_OK) < 0) {
                if(mkdir(name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0)
                    return -1;
            }
        }

        return 0;
    }

    if(stat(path, &pathstat) < 0) {
        printf("stat failed for:%s\n", path);
        return -1;
    }

    if(!S_ISDIR(pathstat.st_mode)) {
        printf("%s is not a directory!!!\n", path);
        return -1;
    }

    return 0;
}


#endif
