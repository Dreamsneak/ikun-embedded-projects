/********************************** (C) COPYRIGHT *******************************
 * File Name          : RF_PHY.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2020/08/06
 * Description        :
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/******************************************************************************/
/* 头文件包含 */
#include "CONFIG.h"
#include "RF_PHY.h"
#include "HAL.h"

/*********************************************************************
 * GLOBAL TYPEDEFS
 */
uint8_t taskID;
uint8_t TX_DATA[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0};

volatile uint8_t tx_end_flag=0;
volatile uint8_t rx_end_flag=0;

/* RF中断回调内置标志与暂存缓冲：中断内只拷贝数据/置标志，等待与打印移至任务上下文 */
#define RF_RX_FLAG_DATA      0x01    /* 收到有效数据，暂存区有效 */
#define RF_RX_FLAG_BADLEN    0x02    /* 包长度非法 */
#define RF_RX_FLAG_CRC       0x04    /* crc错误 */
#define RF_RX_FLAG_TYPE      0x08    /* 匹配类型错误 */
#define RF_RX_FLAG_TXSIDE    0x10    /* 数据来自TX侧收到的应答（TX_MODE_RX_DATA） */
__attribute__((aligned(4))) static uint8_t rf_rx_tmp[256];
static volatile uint8_t rf_rx_flag = 0;

/*********************************************************************
 * @fn      RF_Wait_Tx_End
 *
 * @brief   手动模式等待发送完成，自动模式等待发送-接收完成，必须在RAM中等待，等待时可以执行用户代码，但需要注意执行的代码必须运行在RAM中，否则影响发送
 *
 * @return  none
 */
__HIGH_CODE
__attribute__((noinline))
void RF_Wait_Tx_End(void)
{
    uint32_t i=0;
    while(!tx_end_flag)
    {
        i++;
        __nop();
        __nop();
        // 约5ms超时
        if(i>(FREQ_SYS/1000))
        {
            tx_end_flag = TRUE;
        }
    }
}

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
        case TX_MODE_TX_FINISH:
        {
            break;
        }
        case TX_MODE_TX_FAIL:
        {
            tx_end_flag = TRUE;
            break;
        }
        case TX_MODE_RX_DATA:
        {
            tx_end_flag = TRUE;
            /* 中断内只拷贝数据与置标志，打印移至任务上下文 */
            if (crc == 0) {
                uint8_t len = rxBuf[1];
                if((len + 2) <= sizeof(rf_rx_tmp))
                {
                    tmos_memcpy(rf_rx_tmp, rxBuf, len + 2);
                    rf_rx_flag |= RF_RX_FLAG_DATA | RF_RX_FLAG_TXSIDE;
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
        case TX_MODE_RX_TIMEOUT: // Timeout is about 200us
        {
            tx_end_flag = TRUE;
            break;
        }
        case TX_MODE_HOP_SHUT:
        {
            tx_end_flag = TRUE;
            PRINT("TX_MODE_HOP_SHUT...\n");
            tmos_set_event(taskID, SBP_RF_CHANNEL_HOP_TX_EVT);
            break;
        }

        case RX_MODE_RX_DATA:
        {
            /* 中断内只拷贝数据与置标志，等待与打印移至任务上下文 */
            if (crc == 0) {
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
            tmos_set_event(taskID, SBP_RF_RF_RX_EVT);
            break;
        }
        case RX_MODE_TX_FINISH:
        {
            rx_end_flag = TRUE;
            break;
        }
        case RX_MODE_TX_FAIL:
        {
            rx_end_flag = TRUE;
            break;
        }
        case RX_MODE_HOP_SHUT:
        {
            PRINT("RX_MODE_HOP_SHUT...\n");
            rx_end_flag = TRUE;
            tmos_set_event(taskID, SBP_RF_CHANNEL_HOP_RX_EVT);
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
    if(events & SBP_RF_START_DEVICE_EVT)
    {
        tmos_start_task(taskID, SBP_RF_PERIODIC_EVT, 1000);
        return events ^ SBP_RF_START_DEVICE_EVT;
    }
    if(events & SBP_RF_PERIODIC_EVT)
    {
        RF_Shut();
        TX_DATA[0]--;
        tx_end_flag = FALSE;
        if(!RF_Tx(TX_DATA, 10, 0xFF, 0xFF))
        {
            RF_Wait_Tx_End();
        }
        tmos_start_task(taskID, SBP_RF_PERIODIC_EVT, 1000);
        return events ^ SBP_RF_PERIODIC_EVT;
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
            /* RX侧自动模式：任务上下文等待应答发送完成（带约5ms超时），替代原中断内忙等 */
            if(!(flag & RF_RX_FLAG_TXSIDE) && !rx_end_flag)
            {
                RF_Wait_Rx_End();
            }
            if(flag & RF_RX_FLAG_TXSIDE)
            {
                PRINT("tx recv,rssi:%d\n", (int8_t)rf_rx_tmp[0]);
            }
            else
            {
                PRINT("rx recv, rssi: %d\n", (int8_t)rf_rx_tmp[0]);
            }
            PRINT("len:%d-", rf_rx_tmp[1]);

            for (i = 0; i < rf_rx_tmp[1]; i++) {
                PRINT("%x ", rf_rx_tmp[i + 2]);
            }
            PRINT("\n");
        }
        return events ^ SBP_RF_RX_DEAL_EVT;
    }
    if(events & SBP_RF_RF_RX_EVT)
    {
        uint8_t state;
        RF_Shut();
        TX_DATA[0]++;
        rx_end_flag = FALSE;
        state = RF_Rx(TX_DATA, 10, 0xFF, 0xFF);
        return events ^ SBP_RF_RF_RX_EVT;
    }
    // 开启跳频发送
    if(events & SBP_RF_CHANNEL_HOP_TX_EVT)
    {
        PRINT("\n------------- hop tx...\n");
        if(RF_FrequencyHoppingTx(16))
        {
            tmos_start_task(taskID, SBP_RF_CHANNEL_HOP_TX_EVT, 100);
        }
        else
        {
            tmos_start_task(taskID, SBP_RF_PERIODIC_EVT, 1000);
        }
        return events ^ SBP_RF_CHANNEL_HOP_TX_EVT;
    }
    // 开启跳频接收
    if(events & SBP_RF_CHANNEL_HOP_RX_EVT)
    {
        PRINT("hop rx...\n");
        if(RF_FrequencyHoppingRx(200))
        {
            tmos_start_task(taskID, SBP_RF_CHANNEL_HOP_RX_EVT, 400);
        }
        else
        {

            rx_end_flag = FALSE;
            RF_Rx(TX_DATA, 10, 0xFF, 0xFF);
        }
        return events ^ SBP_RF_CHANNEL_HOP_RX_EVT;
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
    rfConfig_t rfConfig;

    tmos_memset(&rfConfig, 0, sizeof(rfConfig_t));
    taskID = TMOS_ProcessEventRegister(RF_ProcessEvent);
    rfConfig.accessAddress = 0x71764129; // 禁止使用0x55555555以及0xAAAAAAAA ( 建议不超过24次位反转，且不超过连续的6个0或1 )
    rfConfig.CRCInit = 0x555555;
    rfConfig.ChannelMap = 0xFFFFFFFF;
    rfConfig.LLEMode = LLE_MODE_AUTO;
    rfConfig.rfStatusCB = RF_2G4StatusCallBack;
    rfConfig.RxMaxlen = 251;
#if (CLK_OSC32K != 0)
    //It is better to choose a shorter heartbeat interval for the internal clock.
    rfConfig.HeartPeriod = 4;
#endif
    state = RF_Config(&rfConfig);
    PRINT("rf 2.4g init: %x\n", state);
//    { // RX mode
//        PRINT("RX mode...\n");
//        tmos_set_event(taskID, SBP_RF_CHANNEL_HOP_RX_EVT);
//    }
    RF_Wait_Rx_End();
    { // TX mode
        PRINT("TX mode...\n");
        tmos_set_event(taskID, SBP_RF_CHANNEL_HOP_TX_EVT);
    }
}

/******************************** endfile @ main ******************************/
