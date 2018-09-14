
/********************************************************************/
static void Delay()
{
    int i = 20;

    for(i = 0;i < 100;i++)
        dsb();
}
#define SDA 6 
#define SCL 7 

static inline void SET_VALUE(int gpio, int val) 
{
    gpio_tlmm_config(gpio, 0, 1, GPIO_NO_PULL, GPIO_2MA, GPIO_ENABLE);
    gpio_set_val(gpio, val);
}

#define GPIO_INPUT_VAL(gpio) gpio_status(gpio)
#define GPIO_DIRECTION_INPUT(gpio) gpio_tlmm_config(gpio, 0, 0, GPIO_NO_PULL, GPIO_2MA, GPIO_ENABLE)

#define SET_HIGH(gpio) SET_VALUE(gpio, 1)
#define SET_LOW(gpio) SET_VALUE(gpio, 0)

static void I2C_Start(void)
{
    SET_HIGH(SCL);
    SET_HIGH(SDA);
    Delay();
    SET_LOW(SDA);
    Delay();
    SET_LOW(SCL);
    Delay();;
}

static void I2C_SendACK(uint8_t ack)
{
    if (ack == 1)
    {
        SET_HIGH(SDA);
    }
    else
    {
        SET_LOW(SDA);
    }
    Delay();
    SET_HIGH(SCL);
    Delay();
    SET_LOW(SCL);  
    Delay(); 
}

static int I2C_SendByteWaitClockStretch(uint8_t dat)
{
    uint8_t i;
    uint8_t CY;
    int timeout = 100 * 1000;
    for (i = 0; i < 8; i++)
    {                  
        if (dat & 0x80)
        {
            SET_HIGH(SDA);
        }
        else
        {
            SET_LOW(SDA);
        }
        dat <<= 1;
        GPIO_DIRECTION_INPUT(SCL); 
        Delay(); 
        while(GPIO_INPUT_VAL(SCL) == 0 && timeout >= 0) {
            mdelay(1);
            timeout -= 1000;
        }
        SET_HIGH(SCL);
        Delay(); 
        SET_LOW(SCL);
        Delay(); 
    }
    GPIO_DIRECTION_INPUT(SDA); 
    GPIO_DIRECTION_INPUT(SCL); 
    Delay(); 
    while(GPIO_INPUT_VAL(SCL) == 0 && timeout >= 0) {
        mdelay(1);
        timeout -= 1000;
    }
    SET_HIGH(SCL);
    Delay();               
    if(GPIO_INPUT_VAL(SDA))         
    {
        CY = 1;
    }
    else 
	{
		CY = 0;
	}
    SET_LOW(SCL);      
    Delay();                
    if (CY == 0)
	{
		return 0;
	}
	return -1;
}

static uint8_t I2C_RecvByte_ClockStretch()
{
    uint8_t i;
    uint8_t dat = 0;
    int timeout = 100 * 1000;
    GPIO_DIRECTION_INPUT(SDA); 
    for (i=0; i<8; i++)
    {
        dat <<= 1;
        GPIO_DIRECTION_INPUT(SCL); 
        Delay(); 
        while(GPIO_INPUT_VAL(SCL) == 0 && timeout >= 0) {
            mdelay(1);
            timeout -= 1000;
        }
        SET_HIGH(SCL);
        Delay();               
        if(GPIO_INPUT_VAL(SDA))         
        {
            dat |= 0x01;
        }
        else
		{			
			dat |= 0;   
		}       
        SET_LOW(SCL);
        Delay();
    }
    return dat;
}

static void I2C_Stop()
{
    SET_LOW(SDA);
    Delay();
    SET_HIGH(SCL);
    Delay();
    SET_HIGH(SDA);
    Delay();
}

#define BQ27542_SlaveAddress (0xAA)
#define HYM8563_SlaveAddress (0xA2)
#define BQ25601_SlaveAddress (0xD6)

static int writeI2C_ClockStretch(uint8_t dev_address, uint8_t REG_Address,uint8_t REG_data)
{
	int ret = 0;
    I2C_Start(); 
    ret = I2C_SendByteWaitClockStretch(dev_address);
	if (ret < 0)
		goto done;
    ret = I2C_SendByteWaitClockStretch(REG_Address);
	if (ret < 0)
		goto done;
    ret = I2C_SendByteWaitClockStretch(REG_data);
	if (ret < 0)
		goto done;
done:	
    I2C_Stop();
	return ret;
}

static int readI2C_ClockStretch(uint8_t dev_address, uint8_t REG_Address, uint8_t *buf, int size)
{
    int i;
	int ret = 0;
    I2C_Start();
    ret = I2C_SendByteWaitClockStretch(dev_address);
	if (ret < 0)
		goto done;
    ret = I2C_SendByteWaitClockStretch(REG_Address);
	if (ret < 0)
		goto done;
    I2C_Start();
    ret = I2C_SendByteWaitClockStretch(dev_address | 0x01); 
	if (ret < 0)
		goto done;	
    for(i = 0; i < size; i++)
    {
        buf[i] = I2C_RecvByte_ClockStretch(); 
        if(i != size - 1)
            I2C_SendACK(0); 
        else
            I2C_SendACK(1); 
    }
done:	
    I2C_Stop(); 
    return ret;
}

static int restore_i2c(void)
{
	int i;
	for (i = 0; i < 9; i++)
	{
		SET_LOW(SCL);
		Delay();
		SET_LOW(SDA);
		Delay();
		SET_HIGH(SCL);
		Delay();
		SET_HIGH(SDA);
		Delay();		
	}
    GPIO_DIRECTION_INPUT(SDA); 
    if(GPIO_INPUT_VAL(SDA) == 0)	
		return -1;
	return 0;
}

void test_i2c()
{
    uint8_t buf[8];

    GPIO_DIRECTION_INPUT(SDA); 
    if(GPIO_INPUT_VAL(SDA) == 0)
    {
        printf("%s detected a bad loop i2c device\n", __func__);
        if(restore_i2c() < 0)
        {
            printf("%s restore i2c device failed!\n", __func__);
            return;
        }
    }

}
