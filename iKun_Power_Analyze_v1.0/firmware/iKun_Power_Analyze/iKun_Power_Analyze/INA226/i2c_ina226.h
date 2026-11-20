#ifndef __INA226_H
#define __INA226_H

#include "ch32v20x.h"

// INA226 I2C 地址
#define INA226_ADDR        0x44

// 分流电阻 0.1Ω（R100）
// #define INA226_RSHUNT      0.10f         (根据实际电阻配置)
#define INA226_CURRENT_LSB  (0.00512f / (cal_val * INA226_RSHUNT))  // 由 cal 反推

// 正确寄存器地址
#define INA226_REG_CONFIG      0x00
#define INA226_REG_SHUNT_V     0x01
#define INA226_REG_BUS_V       0x02
#define INA226_REG_POWER       0x03
#define INA226_REG_CURRENT     0x04
#define INA226_REG_CALIBRATION 0x05
#define INA226_REG_MAN_ID      0xFE
#define INA226_REG_DIE_ID      0xFF

void I2C1_Init(void);
void INA226_Init(void);

float INA226_Read_ShuntVoltage(void);
float INA226_Read_BusVoltage(void);
float INA226_Read_Current(void);
float INA226_Read_Power(void);

uint8_t INA226_Check(void);
uint16_t INA226_ReadReg(uint8_t reg);
uint8_t I2C_Scan(uint8_t found_addrs[]);

#endif