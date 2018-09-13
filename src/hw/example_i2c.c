#include <stdio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <pthread.h>

#include "example_i2c.h"

#define MAX_I2CDEVS (16)
struct i2c_private {
    char i2cdev[16];
    int fd, users;
};
static struct i2c_private *i2cdevs[16];
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static int i2c_write_data(struct i2c_context *i2c, unsigned char *buf, int size)
{
    int fd = i2c->fd, ret;
    struct i2c_rdwr_ioctl_data rdwr_data;
    struct i2c_msg msg[1];

    rdwr_data.nmsgs = 1;
    rdwr_data.msgs  = msg;

    msg[0].len   = size; 
    msg[0].addr  = i2c->addr; 
    msg[0].flags = 0;//write
    msg[0].buf   = buf;//&reg;

    do {
        ret = ioctl(fd, I2C_RDWR, (unsigned long)&rdwr_data);
    } while(ret < 0 && errno == EINTR);

    return ret < 0 ? ret: 0;
}

static int i2c_read_data(struct i2c_context *i2c, unsigned char *buf, int size)
{
    int fd = i2c->fd, ret;
    struct i2c_rdwr_ioctl_data rdwr_data;
    struct i2c_msg msg[1];

    rdwr_data.nmsgs = 1;
    rdwr_data.msgs  = msg;

    msg[0].len   = size;
    msg[0].addr  = i2c->addr;
    msg[0].flags = I2C_M_RD;//read
    msg[0].buf   = buf;

    do {
        ret = ioctl(fd, I2C_RDWR, (unsigned long)&rdwr_data);
    } while(ret < 0 && errno == EINTR);

    return ret < 0 ? ret: 0;
}

static int i2c_write_reg(struct i2c_context *i2c, unsigned char reg, unsigned char *buf, int size)
{
    int fd = i2c->fd,ret = 0;
    struct i2c_rdwr_ioctl_data rdwr_data;
    struct i2c_msg msg[1];
    unsigned char data[64];

    if(size >= sizeof(data))
        return -1;

    data[0] = reg;
    memcpy(data + 1, buf, size);

    rdwr_data.nmsgs = 1;
    rdwr_data.msgs  = msg;

    msg[0].len   = 1 + size; 
    msg[0].addr  = i2c->addr; 
    msg[0].flags = 0;//write
    msg[0].buf   = data;//&reg;

    do {
        ret = ioctl(fd, I2C_RDWR, (unsigned long)&rdwr_data);
    } while(ret < 0 && errno == EINTR);

    return ret < 0 ? ret: 0;
}

static int i2c_read_reg(struct i2c_context *i2c, unsigned char reg, unsigned char *buf, int size)
{
    int fd = i2c->fd,ret = 0;
    struct i2c_rdwr_ioctl_data rdwr_data;
    struct i2c_msg msg[2];

    rdwr_data.nmsgs = 2;
    rdwr_data.msgs  = msg;

    msg[0].len   = 1; 
    msg[0].addr  = i2c->addr; 
    msg[0].flags = 0;//write
    msg[0].buf   = &reg;

    msg[1].len   = size;
    msg[1].addr  = i2c->addr;
    msg[1].flags = I2C_M_RD;//read
    msg[1].buf   = buf;

    do {
        ret = ioctl(fd, I2C_RDWR, (unsigned long)&rdwr_data);
    } while(ret < 0 && errno == EINTR);

    return ret < 0 ? ret: 0;
}

static int add_i2cdevs(const char *i2c_dev)
{
    struct i2c_private *priv;
    int fd, i;
    
    pthread_mutex_lock(&lock);

    for(i = 0;i < MAX_I2CDEVS;i++) {
        priv = i2cdevs[i];
        if(priv && (strcmp(priv->i2cdev, i2c_dev) == 0)) {
            fd = priv->fd;
            goto found;
        }
    }

    /* not found, create new */
    for(i = 0;i < MAX_I2CDEVS;i++) {
        priv = i2cdevs[i];
        if(priv == NULL) {
            break;
        }
    }

    if(i >= MAX_I2CDEVS) {
        fd = -ENOMEM;
        goto err_nomem;
    }

    priv = calloc(1, sizeof(*priv));
    if(priv == NULL) {
        fd = -ENOMEM;
        goto err_nomem;
    }

    if(strnlen(i2c_dev, sizeof(priv->i2cdev)) == sizeof(priv->i2cdev)) {
        fd = -EINVAL;
        goto err_open;
    }
    strncpy(priv->i2cdev, i2c_dev, sizeof(priv->i2cdev));

    fd = open(i2c_dev, O_RDWR);
    if(fd < 0) {
        goto err_open;
    }
    priv->fd = fd;
    i2cdevs[i] = priv;

found:
    ++priv->users;
    pthread_mutex_unlock(&lock);
    return fd;

err_open:
    free(priv);
err_nomem:
    pthread_mutex_unlock(&lock);
    return fd;
}

static void del_i2cdevs(int fd)
{
    struct i2c_private *priv;
    int i;
    
    pthread_mutex_lock(&lock);

    for(i = 0;i < MAX_I2CDEVS;i++) {
        priv = i2cdevs[i];
        if(priv && (priv->fd == fd)) {
            goto found;
        }
    }

    /* no i2c dev found */
    pthread_mutex_unlock(&lock);
    return;

found:
    if(--priv->users <= 0) {
        close(priv->fd);
        free(priv);
        i2cdevs[i] = NULL;
    }
    pthread_mutex_unlock(&lock);
    return;
}

static void free_i2c_context(struct i2c_context *i2c)
{
    del_i2cdevs(i2c->fd);
    free(i2c);
}

struct i2c_context *init_i2c_context(const char *i2c_dev, int addr)
{
    struct i2c_context *i2c;
    int fd = add_i2cdevs(i2c_dev);

    if(fd < 0)
        return NULL;

    i2c = malloc(sizeof(*i2c));
    if(i2c == NULL) {
        del_i2cdevs(fd);
        return NULL;
    }

    i2c->fd   = fd;
    i2c->addr = addr;
    i2c->write_reg  = i2c_write_reg;
    i2c->read_reg   = i2c_read_reg;
    i2c->write_data = i2c_write_data;
    i2c->read_data  = i2c_read_data;
    i2c->destroy    = free_i2c_context;

    return i2c;
}

