
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include "list.h"
#include "file.h"
#include "hash.h"

#define LOGFILE_HTABLE_SIZE_INBITS (7)
#define LOGFILE_HTABLE_SIZE (1 << LOGFILE_HTABLE_SIZE_INBITS)
#define LOGFILE_HTABLE_MASK (LOGFILE_HTABLE_SIZE - 1)

struct logfile_context {
    char *path;
    int num;
    struct hlist_head *head;
};

static struct logfile_context lfctx;

static inline uint32_t hashfn(char *p, char *g)
{
    uint32_t val = (p[0]) | (p[1] << 8) | (g[0] << 16) | (g[1] << 24);
    return hash_32(val, LOGFILE_HTABLE_SIZE_INBITS);
}

static int new_file(struct logfile_context *ctx, struct log_file *file)
{
    struct hlist_head *head;
    struct hlist_node *n;
    struct stat stat;
    char filename[128];
    int max = 0, i, index, fd;

    while(ctx->num >= MAX_LOGFILES) {
        struct log_file *pos, *q = NULL;
        int  times;

        for(times = 0; times < LOGFILE_CLEAN_STEP;times++) {
            max = 0;
            for(i = 0;i < LOGFILE_HTABLE_SIZE;i++) {
                head = &ctx->head[i];
                hlist_for_each_entry(pos, n, head, list) {
                    if(pos->num > max) {
                        max = pos->num;
                        q = pos;
                    }
                }
            }

            pos = q;
            index = pos->index + 1;
            while(pos) {
                if(++index > MAXLOGS_FOR_ONE)
                    index = 1;

                snprintf(filename, sizeof(filename), "%s.%d", pos->filename, index);
                if(access(filename, F_OK) == 0) 
                   break; 
            }

            if(remove(filename) < 0)
                printf("remove file:%s failed, error:%s!!\n", filename, strerror(errno));
            pos->num--;
            ctx->num--;
        }
    }

    snprintf(filename, sizeof(filename), "%s.%d", file->filename, file->index);
    if(access(filename, F_OK) == 0) {
        if(remove(filename) < 0)
            printf("remove file:%s failed:%s!!\n", filename, strerror(errno));
        file->num--;
        ctx->num--;
    }

    if(file->size >= LOGFILE_MAXSIZE) {
        if(rename(file->filename, filename) < 0) {
            printf("rename file:%s failed:%s!!\n", filename, strerror(errno));
            return -1; 
        }
        file->num++;
        ctx->num++;
    }

    if((fd = open(file->filename, O_RDWR | O_CREAT, 
                  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH)) < 0
                 || lseek(fd, 0, SEEK_END) < 0) {
        printf("open :%s failed!\n", filename);
        return -1;
    }

    if(++file->index > MAXLOGS_FOR_ONE)
        file->index = 0;
    file->fd = fd;

    if(fstat(fd, &stat) < 0) {
        printf("stat failed for:%s\n", filename);
        return -1;
    }

    file->size = stat.st_size;

    return fd;
}

static int write_log(struct log_file *file, char *buf, int len)
{
    int left = len, ret, fd = file->fd;

    while(left > 0) {
        ret = write(fd, buf + len - left, left);
        if(ret < 0) {
            if(errno != EINTR) {
                return -1;
            }
            continue;
        } else
            left -= ret;
    }

    file->size += len;

    if(file->size >= LOGFILE_MAXSIZE) {
        close(file->fd);
        return new_file(&lfctx, file);
    }
    return 0;
}

static int close_log(struct log_file *file)
{
    /* do nothing */
    file = file;
    return 0;
}

static int get_index(char *filename, int *index, int *count)
{
    char buf[128];
    int i = 0, c = 0, found = 0;

    if(access(filename, F_OK) == 0 )
        c++;

    do {
        snprintf(buf, sizeof(buf), "%s.%d", filename, i);
        if(access(buf, F_OK) < 0 ) {
            if(!found)
                *index = i;
            found = 1;
        } else 
            c++;
    } while(++i < MAXLOGS_FOR_ONE);

    if(!found)
        *index = MAXLOGS_FOR_ONE;
    *count = c;

    return 0;
}

static struct log_file *new_log(char *pro, char *group)
{
    struct logfile_context *ctx = &lfctx;
    struct log_file *file;
    struct hlist_head *head;
    char   filename[128];
    int    key, fd;

    key = hashfn(pro, group);
    head = &ctx->head[key];

    file = malloc(sizeof(*file));
    if(!file) {
        printf("%s, malloc failed\n", __func__);
        return NULL;
    }
    memset(file, 0, sizeof(*file));
    file->write = write_log;
    file->close = close_log;

    file->group = strdup(group);
    file->path = strdup(pro);
    snprintf(filename, sizeof(filename), "%s/%s.log", pro, group);
    file->filename = strdup(filename);
    if(!file->path || !file->filename || !file->group) {
        printf("%s, strdup failed\n", __func__);
        return NULL;
    }

    if(check_dir(pro) < 0) {
        printf("%s failed for :%s\n", __func__, pro);
        return NULL;
    }

    hlist_add_head(&file->list, head);
    get_index(filename, &file->index, &file->num);
    ctx->num += file->num;
    printf("%s,get index:%d,num:%d, total:%d\n", filename, file->index, file->num, ctx->num);

    if((fd = new_file(ctx, file)) < 0) {
        printf("open :%s failed!\n", filename);
        return NULL;
    }

    return file;
}

struct log_file *open_logfile(char *pro, char *group)
{
    struct logfile_context *ctx = &lfctx;
    struct log_file *file;
    struct hlist_node *n;
    struct hlist_head *head;
    int key;

    key = hashfn(pro,  group);
    head = &ctx->head[key];
    hlist_for_each_entry(file, n, head, list) {
        if(!strcmp(group, file->group) && !strcmp(pro, file->path)) {
            return file;
        }
    }

    file = new_log(pro, group);
    if(!file) {
        printf("%s, malloc failed\n", __func__);
        return NULL;
    }

    return file;
}

static int scan_subdir(char *path)
{
    DIR *dp = opendir(path);
    struct dirent *dirent;
    char group[64], *ptr;
    int len;

    if(!dp) {
        printf("open directory:%s failed\n", path);
        return 0;
    }

    printf("get new program:%s\n", path);
    while((dirent = readdir(dp))) {

        if(!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
            continue;

        ptr = strrchr(dirent->d_name, '.');
        if(!ptr || strcmp(ptr, ".log"))
            continue;
    
        len = (ptr - dirent->d_name > (int)sizeof(group)) ? (int)sizeof(group) : ptr - dirent->d_name;
        memcpy(group, dirent->d_name, len);
        group[len] = '\0';
    
        printf("pro:%s,group:%s\n", path, group);
        if(!new_log(path, group)) {
            printf("%s,open new log failed!\n", __func__);
            continue;
        }
    }

    closedir(dp);

    return 0;
}

static int init_internal(char *path)
{
    DIR *dp = opendir(path);
    struct dirent *dirent;
    char program[64] = "logd";

    if(chdir(path) < 0) {
        printf("%s,chdir failed !\n", path);
        return -1;
    }

    if(!dp) {
        printf("open directory:%s failed\n", path);
        return 0;
    }

    while((dirent = readdir(dp))) {
        struct stat distat;

        if(!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
            continue;

        if(stat(dirent->d_name, &distat) < 0) {
            printf("stat failed for:%s\n", path);
            return -1;
        }

        if(!S_ISDIR(distat.st_mode)) {
            continue;
        }

        strncpy(program, dirent->d_name, sizeof(program));

        scan_subdir(program);
    }

    closedir(dp);

    return 0;
}

int init_logfile(char *path)
{
    struct logfile_context *ctx = &lfctx;
    int i;

    ctx->head = malloc(sizeof(struct hlist_head) * LOGFILE_HTABLE_SIZE);
    ctx->path = strdup(path);
    if(!ctx->head || !ctx->path) {
        printf("%s malloc failed!!\n", __func__);
        return -ENOMEM;
    }

    for(i = 0;i < LOGFILE_HTABLE_SIZE;i++) {
        INIT_HLIST_HEAD(&ctx->head[i]);
    }

    if(check_dir(path) < 0) {
        printf("%s failed for :%s\n", __func__, path);
        return -1;
    }

    return init_internal(path);
}
