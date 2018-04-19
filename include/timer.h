#ifndef __RCTIMER_H__
#define __RCTIMER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>

#include "timerqueue.h"

#define HRTIMER_STATE_INACTIVE  0x00
#define HRTIMER_STATE_ENQUEUED  0x01
#define HRTIMER_STATE_CALLBACK  0x02

struct timer {
    struct timerqueue_node node;
    unsigned int        state;

    int                 (*fn)(struct timer *timer);
    void                *p;
};

extern struct timerqueue_head timerlist;

static inline int64_t get_current_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

static inline int64_t get_current_time(void)
{
    return get_current_time_us() / 1000;
}

static inline int64_t timer_get_expires(struct timer *t)
{
    return t->node.expires.tv64;
}

static inline void stop_timer(struct timer *timer)
{
    if(timer->state == HRTIMER_STATE_ENQUEUED)
        timerqueue_del(&timerlist, &timer->node);
    timer->state = HRTIMER_STATE_INACTIVE;
}

static inline void start_timer(struct timer *t, int timeout)
{
    if(t->state == HRTIMER_STATE_ENQUEUED)
        timerqueue_del(&timerlist, &t->node);
    
    t->node.expires.tv64 = get_current_time() + (int64_t)timeout;
    timerqueue_add(&timerlist, &t->node);
    t->state = HRTIMER_STATE_ENQUEUED;
}

static inline void update_timer(struct timer *timer, int timeout)
{
    stop_timer(timer);

    timer->node.expires.tv64 = get_current_time() + (int64_t)timeout;
    timerqueue_add(&timerlist, &timer->node);
    timer->state = HRTIMER_STATE_ENQUEUED;
}

static inline void del_timer(struct timer *timer)
{
    stop_timer(timer);
    free(timer);
}

struct timer *add_timer(void *p, int (*func)(struct timer *), int timeout);

int process_timer(void);

#ifdef __cplusplus
}
#endif
#endif	/* __RCTIMER_H__ */
