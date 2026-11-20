/********************************** (C) COPYRIGHT *******************************
 * File Name          : RF_PHY.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2020/08/06
 * Description        : RF_PHY例程：2.4G接收，收到数据后自动应答并打印
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/******************************************************************************/
/* 头文件包含 */
#include "CONFIG.h"
#include "RF_PHY.h"

/*********************************************************************
 * GLOBAL TYPEDEFS
 */

#define RF_AUTO_MODE_EXAM       1

uint8_t taskID;
uint8_t TX_DATA[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0};

volatile uint8_t rx_end_flag=0;

/* RF中断回调内置标志与暂存缓冲：中断内只拷贝数据/置标志，等待与打印移至任务上下文 */
#define RF_RX_FLAG_DATA      0x01    /* 收到有效数据，暂存区有效 */
#define RF_RX_FLAG_BADLEN    0x02    /* 包长度非法 */
#define RF_RX_FLAG_CRC       0x04    /* crc错误 */
#define RF_RX_FLAG_TYPE      0x08    /* 匹配类型错误 */
__attribute__((aligned(4))) static uint8_t rf_rx_tmp[256];
static volatile uint8_t rf_rx_flag = 0;

/*********************************************************************
 * @fn      RF_Wait_Rx_End
 *
 * @brief   自动模式等待应答发送完成，必须在RAM中等待，等待时可以执行用户代码，但需要注意执行的代码必须运行在RAM中，否则影响发送
 *
 * @return  none
 */
__HIGH_CODE
__attribute__((noinline))
void RF_Wait_Rx_End(void)
{
    uint32_t i=0;
    while(!rx_end_flag)
    {
        i++;
        __nop();
        __nop();
        // 约5ms超时
        if(i>(FREQ_SYS/1000))
        {
            rx_end_flag = TRUE;
        }
    }
}

/*********************************************************************
 * @fn      RF_2G4StatusCallBack
 *
 * @brief   RF 状态回调，此函数在中断中调用。注意：不可在此函数中直接调用RF接收或者发送API，需要使用事件的方式调用
 *          在此回调中直接使用或调用函数涉及到的变量需注意，此函数在中断中调用。
 *
 * @param   sta     - 状态类型
 * @param   crc     - crc校验结果
 * @param   rxBuf   - 数据buf指针
 *
 * @return  none
 */
void RF_2G4StatusCallBack(uint8_t sta, uint8_t crc, uint8_t *rxBuf)
{
    switch(sta)
    {
        case RX_MODE_RX_DATA:
        {
            if (crc == 0) {
                /* 中断内只拷贝数据与置标志，等待与打印移至任务上下文 */
                uint8_t len = rxBuf[1];
                if((len + 2) <= sizeof(rf_rx_tmp))
                {
                    tmos_memcpy(rf_rx_tmp, rxBuf, len + 2);
                    rf_rx_flag |= RF_RX_FLAG_DATA;
                }
                else
                {
                    rf_rx_flag |= RF_RX_FLAG_BADLEN;
                }
            } else {
                if (crc & (1<<0)) {
                    rf_rx_flag |= RF_RX_FLAG_CRC;
                }

                if (crc & (1<<1)) {
                    rf_rx_flag |= RF_RX_FLAG_TYPE;
                }
            }
            tmos_set_event(taskID, SBP_RF_RX_DEAL_EVT);
            break;
        }
        case RX_MODE_TX_FINISH:
        {
#if(RF_AUTO_MODE_EXAM)
            rx_end_flag = TRUE;
            tmos_set_event(taskID, SBP_RF_RF_RX_EVT);
#endif
            break;
        }
        case RX_MODE_TX_FAIL:
        {
#if(RF_AUTO_MODE_EXAM)
            rx_end_flag = TRUE;
            tmos_set_event(taskID, SBP_RF_RF_RX_EVT);
#endif
            break;
        }
    }
}

/*********************************************************************
 * @fn      RF_ProcessEvent
 *
 * @brief   RF 事件处理
 *
 * @param   task_id - 任务ID
 * @param   events  - 事件标志
 *
 * @return  未完成事件
 */
uint16_t RF_ProcessEvent(uint8_t task_id, uint16_t events)
{
    if(events & SYS_EVENT_MSG)
    {
        uint8_t *pMsg;

        if((pMsg = tmos_msg_receive(task_id)) != NULL)
        {
            // Release the TMOS message
            tmos_msg_deallocate(pMsg);
        }
        // return unprocessed events
        return (events ^ SYS_EVENT_MSG);
    }
    if(events & SBP_RF_RX_DEAL_EVT)   /* 接收数据处理：等待与打印已从RF中断移至此处，须先于重新打开接收处理 */
    {
        uint8_t flag = rf_rx_flag;
        rf_rx_flag = 0;

        if (flag & RF_RX_FLAG_CRC) {
            PRINT("crc error\n");
        }
        if (flag & RF_RX_FLAG_TYPE) {
            PRINT("match type error\n");
        }
        if (flag & RF_RX_FLAG_BADLEN) {
            PRINT("rx len err\n");
        }
        if (flag & RF_RX_FLAG_DATA)
        {
            uint8_t i;
#if(RF_AUTO_MODE_EXAM)
            /* 自动模式等待应答发送完成：带约5ms超时的标志轮询（原在中断内忙等，现移任务上下文） */
            if(!rx_end_flag)
            {
                RF_Wait_Rx_End();
            }
#endif
            PRINT("rx recv, rssi: %d\n", (int8_t)rf_rx_tmp[0]);
            PRINT("len:%d-", rf_rx_tmp[1]);

            for (i = 0; i < rf_rx_tmp[1]; i++) {
                PRINT("%x ", rf_rx_tmp[i + 2]);
            }
            PRINT("\n");
        }
#if(!RF_AUTO_MODE_EXAM)
        /* 非自动模式由任务上下文重新打开接收 */
        tmos_set_event(taskID, SBP_RF_RF_RX_EVT);
#endif
        return events ^ SBP_RF_RX_DEAL_EVT;
    }
    if(events & SBP_RF_RF_RX_EVT)
    {
        uint8_t state;
        RF_Shut();
        TX_DATA[0]++;
#if(RF_AUTO_MODE_EXAM)
        rx_end_flag = FALSE;
#endif
        state = RF_Rx(TX_DATA, 10, 0xFF, 0xFF);
        PRINT("RX mode.state = %x\n", state);
        return events ^ SBP_RF_RF_RX_EVT;
    }
    return 0;
}

/*********************************************************************
 * @fn      RF_Init
 *
 * @brief   RF 初始化
 *
 * @return  none
 */
void RF_Init(void)
{
    uint8_t    state;
    rfConfig_t rf_Config;

    tmos_memset(&rf_Config, 0, sizeof(rfConfig_t));
    taskID = TMOS_ProcessEventRegister(RF_ProcessEvent);
    rf_Config.accessAddress = 0x71764129; // 禁止使用0x55555555以及0xAAAAAAAA ( 建议不超过24次位反转，且不超过连续的6个0或1 )
    rf_Config.CRCInit = 0x555555;
    rf_Config.Channel = 39;
    rf_Config.Frequency = 2480000;
#if(RF_AUTO_MODE_EXAM)
    rf_Config.LLEMode = LLE_MODE_AUTO;
#else
    rf_Config.LLEMode = LLE_MODE_BASIC | LLE_MODE_EX_CHANNEL; // 使能 LLE_MODE_EX_CHANNEL 表示 选择 rf_Config.Frequency 作为通信频点
#endif
    rf_Config.rfStatusCB = RF_2G4StatusCallBack;
    rf_Config.RxMaxlen = 251;
    state = RF_Config(&rf_Config);
    PRINT("rf 2.4g init: %x\n", state);
    /* 打印演示配置信息 */
    PRINT("\n==============================================\n");
    PRINT("  2.4G RF PHY RX\n");
#if( RF_AUTO_MODE_EXAM )
    PRINT("  mode     : AUTO\n");
#else
    PRINT("  mode     : BASIC\n");
#endif
    PRINT("  freq     : %d.%03d MHz\n", rf_Config.Frequency / 1000, rf_Config.Frequency % 1000);
    PRINT("  channel  : %d\n", rf_Config.Channel);
    PRINT("  addr     : %08x\n", rf_Config.accessAddress);
    PRINT("  rxMaxlen : %d\n", rf_Config.RxMaxlen);
    PRINT("==============================================\n");
    { // RX mode
#if(RF_AUTO_MODE_EXAM)
        rx_end_flag = FALSE;
#endif
        state = RF_Rx(TX_DATA, 10, 0xFF, 0xFF);
        PRINT("RX mode.state = %x\n", state);
    }
}

/******************************** endfile @ main ******************************/
