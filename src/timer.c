
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

#include "timer.h"

struct timerqueue_head timerlist = {
    .head = RB_ROOT,
    .next = NULL,
};

struct timer *add_timer(void *p, int (*fn)(struct timer *), int timeout)
{
    struct timer *t;

    t = (struct timer *)malloc(sizeof(struct timer));
    if (!t) {
        return NULL;
    }

    t->fn = fn;
    t->p = p;
    t->state = HRTIMER_STATE_INACTIVE;

    timerqueue_init(&t->node);
    start_timer(t, timeout);

    return t;
}

int process_timer()
{
    struct timerqueue_node *node;
    struct timer *pos = NULL;
    int64_t curr_time;

    while ((node = timerqueue_getnext(&timerlist))) {
        pos = container_of(node, struct timer, node);

        curr_time = get_current_time();
        if(timer_get_expires(pos) > curr_time)
            break;

        stop_timer(pos);

        if(pos->fn)
            pos->fn(pos);
    }

    return pos ? (int)(timer_get_expires(pos) - curr_time) : 1000;
}

