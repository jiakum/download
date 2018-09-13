#ifndef __EXAMPLE_I2C_H__
#define __EXAMPLE_I2C_H__

struct i2c_context {
    int fd, addr;
    int (*read_reg)(struct i2c_context *i2c, unsigned char reg, unsigned char *buf, int size);
    int (*write_reg)(struct i2c_context *i2c, unsigned char reg, unsigned char *buf, int size);
    int (*write_data)(struct i2c_context *i2c, unsigned char *buf, int size);
    int (*read_data)(struct i2c_context *i2c, unsigned char *buf, int size);
    void (*destroy)(struct i2c_context *i2c);
};

/* return NULL if error happened, check errno to see reasons */
struct i2c_context *init_i2c_context(const char *i2c_dev, int addr);

#endif /* end of __EXAMPLE_I2C_H__ */
