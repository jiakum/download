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

#ifndef HYM8563_H_
#define HYM8563_H_

struct Calendar_t
{
    unsigned char sec;
    unsigned char min;
    unsigned char hour;
    unsigned char day;
    unsigned char month;
    unsigned char year;
    unsigned char weekday;
};

int HYM8563_Init();
int HYM8536_GetDateTime(struct Calendar_t *t);
int HYM8536_SetDateTime(struct Calendar_t *t);
void HYM8563_EnableWatchdog(int b);
void HYM8563_FeedWatchdog();

#endif /* I2C_DEV_H_ */


static int i2c_fd;

static int I2C_Dev_Init(int addr)
{
    i2c_fd=open("/dev/i2c-2",O_RDWR);

    if(i2c_fd < 0)
        return i2c_fd;

    ioctl(i2c_fd,I2C_TIMEOUT,1);
    ioctl(i2c_fd,I2C_RETRIES,2);

    return 0;
}

static int CAM_I2C_Write(int addr, unsigned char reg, unsigned char *buf, int size)
{
    int fd = i2c_fd,ret = 0;
    struct i2c_rdwr_ioctl_data e2prom_data;
    struct i2c_msg msg[1];
    unsigned char data[64];

    if(size >= sizeof(data))
        return -1;

    data[0] = reg;
    memcpy(data + 1, buf, size);
    e2prom_data.nmsgs = 1;
    e2prom_data.msgs = msg;

    (e2prom_data.msgs[0]).len = 1 + size; 
    (e2prom_data.msgs[0]).addr = addr; 
    (e2prom_data.msgs[0]).flags = 0;//write
    (e2prom_data.msgs[0]).buf = data;//&reg;
    ret=ioctl(fd, I2C_RDWR, (unsigned long)&e2prom_data);
    if(ret < 0)
    {
        perror("ioctl error2");
        return ret;
    }
    return 0;
}

static int CAM_I2C_Read(int addr, unsigned char reg, unsigned char *buf, int size)
{
    int fd = i2c_fd,ret = 0;
    struct i2c_rdwr_ioctl_data e2prom_data;
    struct i2c_msg msg[2];

    e2prom_data.nmsgs=2;
    e2prom_data.msgs = msg;

    (e2prom_data.msgs[0]).len=1; 
    (e2prom_data.msgs[0]).addr=addr; 
    (e2prom_data.msgs[0]).flags = 0;//write
    (e2prom_data.msgs[0]).buf = &reg;
    (e2prom_data.msgs[1]).len = size;
    (e2prom_data.msgs[1]).addr = addr;
    (e2prom_data.msgs[1]).flags = I2C_M_RD;//read
    (e2prom_data.msgs[1]).buf = buf;
    ret=ioctl(fd, I2C_RDWR, (unsigned long)&e2prom_data);
    if(ret<0)
    {
        perror("ioctl error2");
        return ret;
    }
    return 0;
}
//RTC 类型: RTC_HYM8653       RTC_PCF85063A

#define RTC_HYM8653

#define  HYM_ADDR          (0xA2 >> 1)
#define TIMER_TIMEOUT      30  //min

static int watchdog_enabled = 0;
static int HYM8563_init_ok = 0;

int HYM8563_Init()
{
    HYM8563_init_ok = 0;
    if (I2C_Dev_Init(HYM_ADDR) == 0)
    {
        HYM8563_init_ok = 1;
        uint8_t val = 0x80;//enable clkout
        CAM_I2C_Write(HYM_ADDR, 0x0D, &val, 1);
        return 0;
    }
    return -1;
}

/**
 * @brief  Converts a 2 digit decimal to BCD format
 * @param  Value: Byte to be converted.
 * @retval Converted byte
 */
static uint8_t ByteToBcd2(uint8_t Value)
{
    uint8_t bcdhigh = 0;

    while (Value >= 10)
    {
        bcdhigh++;
        Value -= 10;
    }
    return  (uint8_t)((uint8_t)(bcdhigh << 4) | Value);
}

/**
 * @brief  Converts from 2 digit BCD to Binary format
 * @param  Value: BCD value to be converted.
 * @retval Converted word
 */
static uint8_t Bcd2ToByte(uint8_t Value)
{
    uint8_t tmp = 0;
    tmp = (uint8_t)((uint8_t)((uint8_t)(Value & (uint8_t)0xF0) >> 4) * (uint8_t)10);
    return (uint8_t)(tmp + (Value & (uint8_t)0x0F));
}

int HYM8536_GetDateTime(struct Calendar_t *t)
{

    uint8_t buf[7] = {0};
    if (HYM8563_init_ok == 0)
        return -1;
    CAM_I2C_Read(HYM_ADDR, 0x02, buf, 7);
    t->sec = Bcd2ToByte(buf[0] & 0x7F);
    t->min = Bcd2ToByte(buf[1] & 0x7F);
    t->hour = Bcd2ToByte(buf[2] & 0x3F);
    t->day = Bcd2ToByte(buf[3] & 0x3F);
    t->weekday = Bcd2ToByte(buf[4] & 0x07);
    t->month = Bcd2ToByte(buf[5] & 0x1F);
    t->year = Bcd2ToByte(buf[6] & 0xFF);
    return 0;
}

int HYM8536_SetDateTime(struct Calendar_t *t)
{
    uint8_t buf[7];

    if (HYM8563_init_ok == 0)
        return -1;
    buf[0] = ByteToBcd2(t->sec);
    buf[1] = ByteToBcd2(t->min);
    buf[2] = ByteToBcd2(t->hour);
    buf[3] = ByteToBcd2(t->day);
    buf[4] = ByteToBcd2(t->weekday);
    buf[5] = ByteToBcd2(t->month);
    buf[6] = ByteToBcd2(t->year);
    CAM_I2C_Write(HYM_ADDR, 0x02, buf, 7);
    return 0;
}

void HYM8563_EnableWatchdog(int b)
{
    int i;
    uint8_t reg_addr[] = {0x01, 0x0E, 0x0F};
    uint8_t data[3];
    if (HYM8563_init_ok == 0)
        return;
    watchdog_enabled = b;
    if (b)
    {
        data[0] = 0x11;
        data[1] = 0x83;  //60S
        data[2] = TIMER_TIMEOUT;   //超时时间
    }
    else
    {
        data[0] = 0;
        data[1] = 0;
        data[2] = 0;
    }
    for (i = 0; i < 3; i++)
        CAM_I2C_Write(HYM_ADDR, reg_addr[i], data + i, 1);
}

void HYM8563_FeedWatchdog()
{
    if (watchdog_enabled == 0 || HYM8563_init_ok == 0)
        return;
    uint8_t data = TIMER_TIMEOUT;
    CAM_I2C_Write(HYM_ADDR, 0xf, &data, 1);
}


int main(int argc, char **argv)
{
    struct Calendar_t cal = {
            0, 16, 17, 2, 8, 18, 4
        };;

    HYM8563_Init();

    if(argc >= 2)
    {
        printf("set datatime,year:%d, month:%d, week:%d, day :%d, time:%02d:%02d:%02d\n"
                , cal.year, cal.month, cal.weekday, cal.day, cal.hour, cal.min, cal.sec);
        HYM8536_SetDateTime(&cal);
    }

    HYM8536_GetDateTime(&cal);

    printf("year:%d, month:%d, week:%d, day :%d, time:%02d:%02d:%02d\n"
            , cal.year, cal.month, cal.weekday, cal.day, cal.hour, cal.min, cal.sec);


    return 0;
}

