
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>

#include "io.h"
#include "packet.h"
#include "timer.h"
#include "protocol.h"
#include "common.h"

struct init_proc {
    int (*func) (int epfd, struct epoll_context *epctx);
    char *name;
};

extern int init_server(int epfd, struct epoll_context *epctx);
#define INIT_PROC(fun)  {.func = fun, .name = #fun}
static const struct init_proc programs[] = {
    INIT_PROC(init_server),
};

int main()
{
    int epfd, ret, i, delay = 100;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    if((epfd = epoll_create1(EPOLL_CLOEXEC)) < 0) {
        printf("epoll create failed!!\n");
        return -1;
    }

    for(i = 0; i < (int)ARRAY_SIZE(programs); i++) {
        if((ret = programs[i].func(epfd, malloc(sizeof(struct epoll_context)))) < 0) {
            printf("%s failed! ret:%d, error: %s", programs[i].name, ret, strerror(errno));
            return -1;
        }
    }


    while(1) {
        struct epoll_event events[16];
        do {
            ret = epoll_wait(epfd, events, ARRAY_SIZE(events), delay);
            if (ret < 0 && errno != EAGAIN && errno != EINTR) {
                printf("epoll wait return failed:%s!!!", strerror(errno));
                return -1;
            }
        } while(ret < 0);

        for(i = 0;i < ret;i++) {
            struct epoll_context *epctx = events[i].data.ptr;

            if(epctx && epctx->callback)
                epctx->callback(&events[i], epctx->data);
            else
                printf("Illegal data! %p,%p\n", epctx, epctx ? epctx->callback : NULL);
        }

        delay = process_timer();
    }

    close(epfd);
    return 0;
}
