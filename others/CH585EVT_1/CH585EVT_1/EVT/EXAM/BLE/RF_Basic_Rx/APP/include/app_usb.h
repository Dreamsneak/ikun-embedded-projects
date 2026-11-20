/********************************** (C) COPYRIGHT *******************************
 * File Name          : app_usb.h
 * Author             : 混分巨兽龙某某
 * Version            : V1.0
 * Date               : 2026/11/07
 * Description        : CH585 USB CDC(虚拟串口)设备接口
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

#ifndef __APP_USB_H__
#define __APP_USB_H__

#include "stdint.h"

/**
 * @brief   USB CDC设备初始化(枚举为虚拟串口)
 *
 * @param   None.
 *
 * @return  None.
 */
void app_usb_init(void);

/**
 * @brief   主机是否已完成配置(串口已打开)
 *
 * @param   None.
 *
 * @return  1 - 已配置; 0 - 未配置
 */
uint8_t app_usb_cfg_ready(void);

/**
 * @brief   端点1 IN是否正在传输
 *
 * @param   None.
 *
 * @return  1 - 忙; 0 - 空闲
 */
uint8_t app_usb_in_busy(void);

/**
 * @brief   通过CDC虚拟串口上传数据(单次最多64字节)
 *
 * @param   pDat - 数据指针
 * @param   len  - 数据长度
 *
 * @return  1 - 已启动传输; 0 - 端点忙,未发送
 */
uint8_t app_usb_send(const uint8_t *pDat, uint16_t len);

/**
 * @brief   主机修改串口参数(line coding)回调
 *
 * @note    由USB中断调用, 应用层仅需保存参数并置标志,
 *          不要在中断内直接操作射频/UART
 *
 * @param   pCoding - 7字节line coding(dwDTERate[4] | dwDTERate.. | 停止位 | 校验 | 数据位)
 *
 * @return  None.
 */
void app_usb_on_line_coding(const uint8_t *pCoding);

#endif
