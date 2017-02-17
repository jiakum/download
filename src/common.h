#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/epoll.h>
        
struct epoll_context {
    int (*callback)(struct epoll_event *event, void *data);
    void *data;
};      

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#ifdef __cplusplus
}
#endif

#endif  /// __COMMON_H__
