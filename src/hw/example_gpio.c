#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>

#include <pthread.h>
#include <time.h>

#include "list.h"
#include "example_gpio.h"

struct epoll_context
{
    int (*func)(struct epoll_event *d, struct epoll_context *epctx);
    void *arg;
};

struct gpio_irq_context
{
    void (*func)(int gpio, void *arg, int value);
    void *arg;
    int gpio, fd;
    struct epoll_context *epctx;
    
    struct list_head list;
};

static int epfd;

/* 
 * write_str: write a string to the sys node 
 * This string must be terminated with '\0'
 */
static int write_str(char *buf, char *file)
{
    int fd = open(file, O_WRONLY), ret;

    if(fd < 0)
        return fd;

    do {
        ret = write(fd, buf, strlen(buf));
    } while((ret < 0) && (errno == EINTR));

    close(fd);
    return (ret < 0) ? ret : 0;
}

/* 
 * read_str: read a string from sys node 
 * This function will add '\0' at the string end 
 */
static int read_str(char *file, char *buf, int max)
{
    int fd = open(file, O_RDONLY), ret;

    if(fd < 0)
        return fd;

    do {
        ret = read(fd, buf, max - 1);
    } while((ret < 0) && (errno == EINTR));

    close(fd);

    if(ret < 0)
        return ret;

    buf[ret] = '\0';
    return 0;
}

static int is_gpio_ok(int gpio)
{
    char path[64];

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", gpio);
    if(access(path, R_OK | W_OK | X_OK))
        return 0;

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio);
    if(access(path, R_OK | W_OK))
        return 0;

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    if(access(path, R_OK))
        return 0;

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/edge", gpio);
    if(access(path, R_OK | W_OK))
        return 0;

    return 1;
}

int request_gpio(int gpio, int dir)
{
    char path[64];
    char *strdir = (dir == GPIO_DIRECTION_OUT) ? "out" : "in";

    if(is_gpio_ok(gpio) == 0) 
    {
        char num[16];
        snprintf(num, sizeof(num), "%d", gpio);
        snprintf(path, sizeof(path), "/sys/class/gpio/export");

        if(write_str(num, path) < 0 || is_gpio_ok(gpio) == 0)
            return -1;
    }

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio);
    return write_str(strdir, path);
}

int gpio_direction_output(int gpio, int value)
{
    char path[64];
    int ret;

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio);
    ret = write_str("out", path);
    if(ret < 0)
        return ret;

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    return write_str(value ? "1" : "0", path);
}

int gpio_set_value(int gpio, int value)
{
    char path[64];

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    return write_str(value ? "1" : "0", path);
}

int gpio_direction_input(int gpio)
{
    char path[64], value[8];
    int ret;

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio);
    ret = write_str("in", path);
    if(ret < 0)
        return ret;

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    if(read_str(path, value, sizeof(value)) < 0)
        return -1;

    return strtol(value, NULL, 0);
}

int gpio_get_value(int gpio)
{
    char path[64], value[8];

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    if(read_str(path, value, sizeof(value)) < 0)
        return -1;

    return strtol(value, NULL, 0);
}

static int gpio_epoll_cb(struct epoll_event *d, struct epoll_context *epctx)
{
    struct gpio_irq_context *gctx = epctx->arg;
    char value[8];
    int ret;

    if(d->events & (EPOLLHUP | EPOLLERR))
    {
        /* FIXME!!! EPOLLERR always comes with EPOLLPRI ?! */
    }

    if(d->events & EPOLLPRI)
    {
        lseek(gctx->fd, 0, SEEK_SET);
        do {
            ret = read(gctx->fd, value, sizeof(value) - 1);
        } while(ret < 0 && errno == EINTR);

        if(ret < 0)
            return ret;

        value[ret] = '\0';
        gctx->func(gctx->gpio, gctx->arg, strtol(value, NULL, 0));
    }

    return 0;
}

static char *trigger_type[] = 
{
    "none",
    "falling",
    "rising",
    "both"
};

static struct list_head gpio_irq_head = LIST_HEAD_INIT(gpio_irq_head);
static pthread_mutex_t lock;

int free_gpio_irq(int gpio)
{
    struct list_head *pos, *tmp;
    char path[64];

    pthread_mutex_lock(&lock);
    list_for_each_safe(pos, tmp, &gpio_irq_head) {
        struct gpio_irq_context *gctx = list_entry(pos, struct gpio_irq_context, list);

        if(gctx->gpio == gpio) {

            list_del(pos);
            pthread_mutex_unlock(&lock);

            epoll_ctl(epfd, EPOLL_CTL_DEL, gctx->fd, NULL);

            snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/edge", gpio);
            write_str(trigger_type[GPIO_IRQ_NONE], path);
            close(gctx->fd);
            free(gctx->epctx);
            free(gctx);

            return 0;
        }
    }

    pthread_mutex_unlock(&lock);
    return -EINVAL;
}

static int gpio_irq_initialized = 0;
static int epoll_event_loop(int fd, int maxevent);
static void *gpio_irq_thread(void *arg)
{
    epoll_event_loop(epfd, 64);

    /* never got here*/
    return NULL;
}

int request_gpio_irq(int gpio, int type, void (*cb)(int , void *, int), void *arg)
{
    struct epoll_context     *epctx;
    struct gpio_irq_context  *gctx;
    struct epoll_event d;
    char path[64];
    int fd, ret;

    if(gpio_irq_initialized == 0) {
        pthread_t id;
        epfd = epoll_create1(EPOLL_CLOEXEC);

        if(epfd < 0) {
            return epfd;
        }

        if(pthread_create(&id, NULL, gpio_irq_thread, NULL) < 0) {
            close(epfd);
            return -ENOMEM;
        }
        pthread_detach(id);
        gpio_irq_initialized = 1;
    }

    if(is_gpio_ok(gpio) == 0)
        return -EBUSY;

    if(type <= 0 || type >= (int)(sizeof(trigger_type)/sizeof(trigger_type[0])))
        return -EINVAL;

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/edge", gpio);
    if(write_str(trigger_type[type], path) < 0)
        return -EIO;

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return -EIO;

    do {
        ret = read(fd, path, sizeof(path));
    } while(ret < 0 && errno == EINTR);

    if(ret < 0)
    {
        close(fd);
        return -EIO;
    }

    epctx = malloc(sizeof(*epctx));
    gctx  = malloc(sizeof(*gctx));
    if(!epctx || !gctx)
    {
        close(fd);
        return -ENOMEM;
    }

    gctx->gpio = gpio;
    gctx->func = cb;
    gctx->arg  = arg;
    gctx->fd   = fd;
    gctx->epctx= epctx;

    epctx->func = gpio_epoll_cb;
    epctx->arg  = gctx;

    d.data.ptr = epctx;
    d.events   = EPOLLPRI;

    if(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &d) < 0)
    {
        close(fd);
        free(gctx);
        free(epctx);
        return -ENOMEM;
    }
    pthread_mutex_lock(&lock);
    list_add(&gctx->list, &gpio_irq_head);
    pthread_mutex_unlock(&lock);

    return 0;
}

static int epoll_event_loop(int fd, int maxevent)
{
    struct epoll_event *event;
    int ret, i;

    if(maxevent <= 0 || maxevent > 65536)
        return -EINVAL;

    event = malloc(maxevent * sizeof(*event));

    if(!event)
        return -ENOMEM;

    while(1) 
    {
        ret = epoll_wait(fd, event, maxevent, -1);
        if(ret < 0)
        {
            if(errno == EINTR)
                continue;

            return ret;
        }

        for(i = 0;i < ret;i++)
        {
            struct epoll_context *epctx = event[i].data.ptr;

            if(epctx->func)
                epctx->func(event + i, epctx);
        }
    }

    return 0;
}


