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


#define WARN_ONCE(format, ...) ({          \
    static int __warned;     \
    if (!__warned) {             \
        printf(format, ##__VA_ARGS__);            \
        __warned = 1;            \
    } \
})

#define BUG_ON(condition) do { \
        if(condition) \
            printf("BUG:%s,%s,%d,condition:%s\n", __func__, __FILE__, __LINE__, #condition); \
    } while(0) 

#ifdef __cplusplus
}
#endif

#endif  /// __COMMON_H__
