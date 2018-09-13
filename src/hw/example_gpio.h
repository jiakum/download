#ifndef __EXAMPLE_GPIO_H__
#define __EXAMPLE_GPIO_H__

#ifdef __cplusplus
extern "C"
{
#endif


enum {
    GPIO_DIRECTION_IN,
    GPIO_DIRECTION_OUT,
};

enum {
    GPIO_IRQ_NONE,
    GPIO_IRQ_FALLING = 1,
    GPIO_IRQ_RISING  = 2,
    GPIO_IRQ_BOTH    = 3
};

int request_gpio(int gpio, int dir);
int free_gpio_irq(int gpio);
int request_gpio_irq(int gpio, int type, void (*cb)(int gpio, void *data, int value), void *arg);
int gpio_direction_output(int gpio, int value);
int gpio_set_value(int gpio, int value);
int gpio_get_value(int gpio);
int gpio_direction_input(int gpio);


#ifdef __cplusplus
}
#endif

#endif
