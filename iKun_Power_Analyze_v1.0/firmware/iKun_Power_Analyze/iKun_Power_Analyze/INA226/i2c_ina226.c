/*********************************************************************
 * File Name          : i2c_ina226.c
 * Author             : 混分巨兽龙某某
 * Version            : V1.0.0
 * Date               : 2026/4/29
 * Description        : Software I2C driver and INA226 power monitor chip driver
*******************************************************************************/

#include "i2c_ina226.h"

/************************* I2C Pin Definition *************************/
#define SCL_PIN         GPIO_Pin_1
#define SDA_PIN         GPIO_Pin_0
#define SCL_PORT        GPIOA
#define SDA_PORT        GPIOA

/**
 * @brief Microsecond delay for software I2C timing
 */
#define I2C_DELAY()     { for(uint8_t i=0; i<50; i++) __NOP(); }

/**
 * @brief Set I2C SCL pin high level
 */
#define SCL_HIGH()      GPIO_SetBits(SCL_PORT, SCL_PIN)

/**
 * @brief Set I2C SCL pin low level
 */
#define SCL_LOW()       GPIO_ResetBits(SCL_PORT, SCL_PIN)

/**
 * @brief Set I2C SDA pin high level
 */
#define SDA_HIGH()      GPIO_SetBits(SDA_PORT, SDA_PIN)

/**
 * @brief Set I2C SDA pin low level
 */
#define SDA_LOW()       GPIO_ResetBits(SDA_PORT, SDA_PIN)

/**
 * @brief Read SDA pin logic level
 * @retval 1: High level, 0: Low level
 */
#define SDA_READ()      GPIO_ReadInputDataBit(SDA_PORT, SDA_PIN)

/**
 * @brief Initialize software I2C peripheral
 * @note    Use PA0 as SCL, PA1 as SDA, open-drain output mode
 */
void I2C1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* Enable GPIOA clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    /* Configure SCL pin: Open drain output, 50MHz speed */
    GPIO_InitStructure.GPIO_Pin = SCL_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SCL_PORT, &GPIO_InitStructure);

    /* Configure SDA pin: Open drain output, 50MHz speed */
    GPIO_InitStructure.GPIO_Pin = SDA_PIN;
    GPIO_Init(SDA_PORT, &GPIO_InitStructure);

    /* Default state: SCL and SDA pull high */
    SCL_HIGH();
    SDA_HIGH();
}

/**
 * @brief Generate I2C start signal
 */
static void I2C_Start(void)
{
    SDA_HIGH();
    SCL_HIGH();
    I2C_DELAY();
    
    SDA_LOW();
    I2C_DELAY();
    
    SCL_LOW();
    I2C_DELAY();
}

/**
 * @brief Generate I2C stop signal
 */
static void I2C_Stop(void)
{
    SDA_LOW();
    SCL_HIGH();
    I2C_DELAY();
    
    SDA_HIGH();
    I2C_DELAY();
}

/**
 * @brief Send I2C ACK response
 */
static void I2C_SendACK(void)
{
    SDA_LOW();
    I2C_DELAY();
    
    SCL_HIGH();
    I2C_DELAY();
    
    SCL_LOW();
    SDA_HIGH();
    I2C_DELAY();
}

/**
 * @brief Send I2C NACK response
 */
static void I2C_SendNACK(void)
{
    SDA_HIGH();
    I2C_DELAY();
    
    SCL_HIGH();
    I2C_DELAY();
    
    SCL_LOW();
    I2C_DELAY();
}

/**
 * @brief Wait for ACK signal from slave device
 * @retval 1: ACK received, 0: No ACK / timeout
 */
static uint8_t I2C_WaitACK(void)
{
    uint8_t timeout = 255;
    
    SDA_HIGH();
    I2C_DELAY();
    
    SCL_HIGH();
    I2C_DELAY();
    
    while(SDA_READ() && timeout)
    {
        timeout--;
        I2C_DELAY();
    }
    
    SCL_LOW();
    I2C_DELAY();
    
    return (timeout > 0) ? 1 : 0;
}

/**
 * @brief Send one byte data via I2C bus
 * @param data: Byte data to transmit
 */
static void I2C_SendByte(uint8_t data)
{
    for(uint8_t i=0; i<8; i++)
    {
        if(data & 0x80)
            SDA_HIGH();
        else
            SDA_LOW();
        
        data <<= 1;
        I2C_DELAY();
        
        SCL_HIGH();
        I2C_DELAY();
        
        SCL_LOW();
        I2C_DELAY();
    }
    SDA_HIGH();
}

/**
 * @brief Read one byte data from I2C bus
 * @param ack: 1=Send ACK after read, 0=Send NACK after read
 * @retval Received byte data
 */
static uint8_t I2C_ReadByte(uint8_t ack)
{
    uint8_t data = 0;
    
    SDA_HIGH();
    
    for(uint8_t i=0; i<8; i++)
    {
        data <<= 1;
        SCL_HIGH();
        I2C_DELAY();
        
        if(SDA_READ())
            data |= 0x01;
        
        SCL_LOW();
        I2C_DELAY();
    }
    
    if(ack)
        I2C_SendACK();
    else
        I2C_SendNACK();
    
    return data;
}

/**
 * @brief Write 16-bit value to INA226 register
 * @param reg: INA226 register address
 * @param data: 16-bit data to write
 * @retval 1: Success, 0: Failed
 */
static uint8_t INA226_WriteReg(uint8_t reg, uint16_t data)
{
    I2C_Start();
    
    /* Send slave address + write bit */
    I2C_SendByte(INA226_ADDR << 1);
    if(!I2C_WaitACK())
    {
        I2C_Stop();
        return 0;
    }
    
    /* Send register address */
    I2C_SendByte(reg);
    if(!I2C_WaitACK())
    {
        I2C_Stop();
        return 0;
    }
    
    /* Send high 8-bit data */
    I2C_SendByte((data >> 8) & 0xFF);
    if(!I2C_WaitACK())
    {
        I2C_Stop();
        return 0;
    }
    
    /* Send low 8-bit data */
    I2C_SendByte(data & 0xFF);
    if(!I2C_WaitACK())
    {
        I2C_Stop();
        return 0;
    }
    
    I2C_Stop();
    return 1;
}

/**
 * @brief Read 16-bit value from INA226 register
 * @param reg: INA226 register address
 * @retval Register value, 0xFFFF means communication failed
 * @note Read high byte first, then low byte
 */
uint16_t INA226_ReadReg(uint8_t reg)
{
    uint16_t val = 0xFFFF;
    
    I2C_Start();
    I2C_SendByte(INA226_ADDR << 1);
    if(!I2C_WaitACK()) goto exit;
    
    I2C_SendByte(reg);
    if(!I2C_WaitACK()) goto exit;
    
    /* Restart I2C for read operation */
    I2C_Start();
    I2C_SendByte((INA226_ADDR << 1) | 0x01);
    if(!I2C_WaitACK()) goto exit;
    
    uint8_t high = I2C_ReadByte(1);  /* Read high byte, send ACK */
    uint8_t low  = I2C_ReadByte(0);  /* Read low byte, send NACK */
    
    val = ((uint16_t)high << 8) | low;

exit:
    I2C_Stop();
    return val;
}

// 修正为实际采样电阻 + 电压补偿系数
#define INA226_VOLT_COMP_K    0.94946f
#define INA226_RSHUNT         0.083503f  // 实测真实采样电阻

void INA226_Init(void)
{
    INA226_WriteReg(INA226_REG_CONFIG, 0x4127);
    // 如需极致精准：重新根据真实Rshunt计算校准值，这里先保留原有配置
    uint16_t cal = 2015;
    INA226_WriteReg(INA226_REG_CALIBRATION, cal);
}

float INA226_Read_ShuntVoltage(void)
{
    int16_t raw = INA226_ReadReg(INA226_REG_SHUNT_V);
    return raw * 2.5f / 1000.0f;
}

float INA226_Read_BusVoltage(void)
{
    uint16_t raw = INA226_ReadReg(INA226_REG_BUS_V);
    if(raw == 0xFFFF) return 0.0f;
    float v_raw = raw * 1.25f / 1000.0f;
    return v_raw * INA226_VOLT_COMP_K;
}

// 电流函数：仅使用真实Rshunt，不再额外乘电流系数
float INA226_Read_Current(void)
{
    int16_t shunt_raw = INA226_ReadReg(INA226_REG_SHUNT_V);
    return (shunt_raw * 2.5e-6f) / INA226_RSHUNT;
}

float INA226_Read_Power(void)
{
    // 推荐直接用补偿后的 V*I，精度最高
    float v = INA226_Read_BusVoltage();
    float i = INA226_Read_Current();
    return v * i;
}

/**
 * @brief Check INA226 communication status
 * @retval 1: Chip detected and communication ok, 0: Communication error
 */
uint8_t INA226_Check(void)
{
    uint16_t id = INA226_ReadReg(0xFF); /* Read chip ID register */
    if(id == 0x2260) {
        return 1;
    } else {
        return 0;
    }
}

/**
 * @brief Scan all valid 7-bit I2C addresses on bus
 * @param found_addrs: Array to store found slave addresses
 * @retval Total number of detected I2C devices
 * @note Scan range: 0x08 ~ 0x77, skip reserved addresses
 */
uint8_t I2C_Scan(uint8_t found_addrs[])
{
    uint8_t count = 0;
    
    printf("Scanning I2C bus...\r\n");
    
    for(uint8_t addr = 0x08; addr <= 0x77; addr++)
    {
        I2C_Start();
        
        /* Send slave address (write operation) */
        I2C_SendByte(addr << 1);
        
        uint8_t timeout = 255;
        SDA_HIGH();
        I2C_DELAY();
        SCL_HIGH();
        I2C_DELAY();
        
        /* Poll ACK signal */
        while(SDA_READ() && timeout)
        {
            timeout--;
            I2C_DELAY();
        }
        
        SCL_LOW();
        I2C_Stop();
        
        if(timeout > 0)
        {
            if(found_addrs != NULL && count < 128)
            {
                found_addrs[count] = addr;
            }
            count++;
            printf("  Found device at 0x%02X\r\n", addr);
        }
    }
    
    if(count == 0)
    {
        printf("  No I2C devices found!\r\n");
    }
    else
    {
        printf("  Total %d devices found\r\n", count);
    }
    
    return count;
}