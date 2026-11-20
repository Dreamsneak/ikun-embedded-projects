/********************************** (C) COPYRIGHT *******************************
* File Name          : rf_test.h
* Author             : 混分巨兽龙某某
* Version            : V1.0
* Date               : 2022/03/10
* Description        : 
*******************************************************************************/

#ifndef __RF_TEST_H
#define __RF_TEST_H

#ifdef __cplusplus
extern "C"
{
#endif
#include "wchrf.h"

/*********************************************************************
 * USB无线串口角色配置
 *
 * RF_USB_SERIAL_ROLE 选择本固件烧录的角色:
 *   RF_SERIAL_ROLE_RX: 接收端, 2.4G接收 -> USB CDC虚拟串口 -> PC
 *   RF_SERIAL_ROLE_TX: 发送端, UART1(PA8/PA9) -> 2.4G发送
 *
 * 两端的数据包格式(基本模式空中包: 库发送/接收字节数 = 长度 + 2):
 *   数据包: [type=0x55][长度][序号][payload...][异或校验]
 *   配置包: [type=0x56][长度=9][序号][line coding(7)][异或校验]
 *********************************************************************/
#define RF_SERIAL_ROLE_RX        0
#define RF_SERIAL_ROLE_TX        1

#ifndef RF_USB_SERIAL_ROLE
#define RF_USB_SERIAL_ROLE       RF_SERIAL_ROLE_RX
#endif

#define RF_USB_SERIAL_UART_BAUD  115200  // 发送端UART1默认波特率
#define RF_USB_SERIAL_MAX_PAYLOAD 200    // 单包最大有效载荷
#define RF_USB_SERIAL_PKT_TYPE   0x55    // 数据包类型

/* 波特率配置包: 接收端把主机设置的line coding通过2.4G下发给发送端 */
#define RF_CFG_PKT_TYPE          0x56    // 配置包类型
#define RF_CFG_LINE_CODING_LEN   7       // CDC line coding长度
#define RF_CFG_SEND_TIMES        5       // 配置包发送次数(含首次)
#define RF_CFG_RETRY_MS          15      // 配置包重发间隔(ms)
#define RF_CFG_RX_TIMEOUT        20000   // 发送端监听超时(N*0.5us)=10ms
#define RF_CFG_BAUD_MIN          1200    // 允许下发的最小波特率
#define RF_CFG_BAUD_MAX          3000000 // 允许下发的最大波特率

#define RF_RX_HEAD_SCAN          8       // 接收缓冲帧头搜索范围(兼容RSSI+长度前缀)

#define RF_TEST_SLEEP_EVENT      (1<<0)
#define RF_START_RX_EVENT        (1<<1)
#define RF_TX_POLL_EVENT         (1<<2)
#define RF_CFG_POLL_EVENT        (1<<3)

#define RF_STOP_BOUND_EVENT      (1<<11)
#define RF_START_BOUND_EVENT     (1<<12)

#define RF_TEST_RX_EVENT         (1<<13)
#define RF_TEST_TX_EVENT         (1<<14)

void RFRole_Init(void);

/**
 * @brief   无线串口数据处理(需在主循环中调用)
 *
 * @param   None.
 *
 * @return  None.
 */
void RF_Serial_Process(void);

#ifdef __cplusplus
}
#endif

#endif
