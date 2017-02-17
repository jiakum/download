#ifndef __RCTIMER_H__
#define __RCTIMER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>

#include "list.h"

struct timer {
    struct list_head    list;
    int64_t             timeout;    // in millinsecends

    int                 (*func)(struct timer *timer);
    void                *p;
};

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

static inline void update_timer(struct timer *timer, int timeout)
{
    timer->timeout = get_current_time() + (int64_t)timeout;
}

static inline void stop_timer(struct timer *timer)
{
    timer->timeout = INT64_MAX;
}

static inline void del_timer(struct timer *timer)
{
    list_del(&timer->list);
    free(timer);
}

struct timer *add_timer(void *p, int (*func)(struct timer *), int timeout);

int process_timer(void);

#ifdef __cplusplus
}
#endif
#endif	/* __RCTIMER_H__ */
