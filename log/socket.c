
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>

#include "log.h"
#include "list.h"
#include "file.h"
#include "common.h"

#define RCLOG_DEAFULT_HTABLESIZE (16)
#define RCLOG_HASH_MASK (RCLOG_DEAFULT_HTABLESIZE - 1)
static inline int hashfn(char *name)
{
    unsigned int val = name[0] ^ name[1];
    return val & (RCLOG_HASH_MASK);
}

struct log_head {
    char funname[64];
    struct hlist_head *heads;
    pthread_spinlock_t *lock;
};
static struct log_head lhead;

struct local_logger {
    char group[64];
    int  fd;
    struct hlist_node list;
};

static int connect_socket(char *program, char *group)
{
    int fd;
    struct sockaddr_un addr;
    struct sockaddr_un local;

    fd = socket(PF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if(fd < 0) {
        printf("create local socket failed,errno:%d\n", errno);
        return -1;
    }

    local.sun_family = AF_UNIX;
    snprintf(local.sun_path, sizeof(local.sun_path), "/tmp/logd_client/%s", program);
    if(check_dir(local.sun_path) < 0) {
        printf("make dir :%s failed!\n", local.sun_path);
        return -1;
    }
    snprintf(local.sun_path, sizeof(local.sun_path), "/tmp/logd_client/%s/%s", program, group);
    unlink(local.sun_path);
    if(bind(fd, (const struct sockaddr *)&local, sizeof(local)) < 0) {
        printf("bind local socket:%s failed\n", local.sun_path);
        close(fd);
        return -1;
    }

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, RCLOGD_SOCKET, sizeof(addr.sun_path));
    if(connect(fd, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

    return fd;
}

static int get_logd_connection(char *group)
{
    struct local_logger *logger;
    struct hlist_node *n;
    struct hlist_head *head;
    int key, found = 0;

    if(!group)
        group = "%$";

    key = hashfn(group);
    head = &lhead.heads[key];

    pthread_spin_lock(&lhead.lock[key]);
    hlist_for_each_entry(logger, n, head, list) {
        if(!strncmp(logger->group, group, sizeof(logger->group))) {
            found = 1;
            break;
        }
    }
    pthread_spin_unlock(&lhead.lock[key]);

    if(!found) {
        int fd;

        logger = malloc(sizeof(*logger));
        if(!logger) {
            printf("malloc failed!!!\n");
            return -ENOMEM;
        }

        fd = connect_socket(lhead.funname, group);
        if(fd < 0) {
            free(logger);
            return -1;
        }
        logger->fd = fd;

        strncpy(logger->group, group, sizeof(logger->group));

        pthread_spin_lock(&lhead.lock[key]);
        hlist_add_head(&logger->list, head);
        pthread_spin_unlock(&lhead.lock[key]);
    }

    return logger->fd;
}

int send_log(char *group, char *data, int len)
{
    int left = len, fd, ret;

    fd = get_logd_connection(group);
    if(fd < 0) {
        WARN_ONCE("fatal error!logd connection failed!");
        goto failed;
    }

    while(left > 0) {
        ret = send(fd, data + len -left, left, 0);
        if(ret < 0) {
            if(errno == EINTR)
                continue;
            goto failed;
        }

        left -= ret;
    }

    return 0;

failed:
    if(errno == ENOMEM)
        return -1;
    else {
        struct hlist_head *head;
        struct hlist_node *n;
        struct local_logger *logger;
        int key, found = 0;

        if(fd >= 0)
            close(fd);
        if(!group)
            group = "%$";

        key = hashfn(group);
        head = &lhead.heads[key];
        pthread_spin_lock(&lhead.lock[key]);
        hlist_for_each_entry(logger, n, head, list) {
            if(!strncmp(logger->group, group, sizeof(logger->group))) {
                found = 1;
                break;
            }
        }
        pthread_spin_unlock(&lhead.lock[key]);

        if(found)
            logger->fd = connect_socket(lhead.funname, group);

        return 0;
    }
}

int init_local_logger(char *program)
{
    char *name = strrchr(program, '/');
    int i;

    if(!name)
        name = program;
    else
        name++;
    strncpy(lhead.funname, name, sizeof(lhead.funname));

    lhead.heads = malloc(RCLOG_DEAFULT_HTABLESIZE * sizeof(struct hlist_head));
    lhead.lock = malloc(RCLOG_DEAFULT_HTABLESIZE * sizeof(pthread_spinlock_t));
    if(!lhead.heads || !lhead.lock) {
        printf("malloc failed!!!\n");
        return -ENOMEM;
    };
    for(i = 0;i < RCLOG_DEAFULT_HTABLESIZE;i++) {
        INIT_HLIST_HEAD(&lhead.heads[i]);
        pthread_spin_init(&lhead.lock[i], 0);
    }

    printf("%s success, program:%s\n", __func__, program);

    return 0;
}
