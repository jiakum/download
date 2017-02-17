
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

#include "list.h"
#include "timer.h"

static struct timer timerlist = {
    .list       =       LIST_HEAD_INIT(timerlist.list),
};

struct timer *add_timer(void *p, int (*func)(struct timer *), int timeout)
{
    struct timer *timer = (struct timer *)malloc(sizeof(*timer));

    if (!timer) {
        printf("malloc failed for timer\n");
        return NULL;
    }

    timer->func = func;
    timer->p = p;
    timer->timeout = get_current_time() + (int64_t)timeout;

    list_add(&timer->list, &timerlist.list);

    return timer;
}

int process_timer()
{
    struct timer *pos, *next;
    int64_t curr_time = get_current_time();
    int64_t min = curr_time + 100;

    list_for_each_entry_safe(pos, next, &timerlist.list, list) {
        if (pos->timeout <= curr_time) {
            pos->func(pos);
        }

        if (pos->timeout > curr_time && pos->timeout < min) {
            min = pos->timeout;
        }
    }

    return (int)(min - curr_time);
}

