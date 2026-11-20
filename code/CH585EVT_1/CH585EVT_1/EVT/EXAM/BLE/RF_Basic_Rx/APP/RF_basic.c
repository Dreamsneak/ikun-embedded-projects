/********************************** (C) COPYRIGHT *******************************
 * File Name          : rf_basic.c
 * Author             : 混分巨兽龙某某
 * Version            : V1.2
 * Date               : 2026/11/11
 * Description        : 2.4G库基本模式USB无线串口
 *
 *                      接收端(RF_SERIAL_ROLE_RX, 默认):
 *                          2.4G接收 -> 环形缓冲 -> USB CDC虚拟串口 -> PC
 *                          主机修改串口波特率时, 通过2.4G下发配置包给发送端
 *                      发送端(RF_SERIAL_ROLE_TX):
 *                          UART1(PA8-RX, PA9-TX) -> 环形缓冲 -> 2.4G发送
 *                          空闲时监听2.4G, 收到配置包后按新波特率工作
 *
 *                      两端通信参数(接入地址/CRC/频点/PHY)必须一致.
 *                      基本模式空中包只有2字节包头: [type][length][data...],
 *                      库实际发送/接收的字节数 = length + 2, 数据从偏移2开始.
 *                      数据包: [0x55][长度][序号][payload...][异或校验]
 *                      配置包: [0x56][长度=9][序号][line coding(7)][异或校验]
 *
 *                      * 重传机制: sendCount=3 硬件自动重发 + 序号去重
 *                      * 丢包检测: 接收端记录期望序列号, 发现跳变时打印告警
 *
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

/******************************************************************************/
/* 头文件包含 */
#include "CONFIG.h"
#include <string.h>
#include "rf_test.h"

#if (RF_USB_SERIAL_ROLE == RF_SERIAL_ROLE_RX)
#include "app_usb.h"
#endif

/*********************************************************************
 * MACROS
 */
#define TEST_FREQUENCY          16              // 通信频点
#define TEST_ACCESS_ADDRESS     0x71762345      // 接入地址
#define TEST_CRC_INIT           0x555555        // CRC初始值
#define TEST_PKT_HEAD_LEN       2               // 基本模式包头: [type][length]
#define TEST_PKT_DATA_OFS       (TEST_PKT_HEAD_LEN + 1)  // 数据偏移(包头+1字节序号)

#define SERIAL_FIFO_SIZE        1024            // 收发缓冲大小,必须为2的幂
#define SERIAL_FIFO_MASK        (SERIAL_FIFO_SIZE - 1)

#define RF_TX_RETRY_COUNT       3               // 每包硬件自动重发次数(含首次)
#define RF_TX_RETRY_INTERVAL    (1999 * 2)      // 重发间隔 N*0.5us = ~2ms

/*********************************************************************
 * GLOBAL TYPEDEFS
 */
tmosTaskID rfTaskID;
rfRoleParam_t gParm;

/* 环形缓冲区: 中断中写入, 主循环中读取 */
static volatile uint16_t s_fifoHead;
static volatile uint16_t s_fifoTail;
static uint8_t s_fifoBuf[SERIAL_FIFO_SIZE];

/* DMA包头偏移(两端共享, 首次收到包时自动探测) */
static uint8_t s_rxPktOffset = 0;
static uint8_t s_rxDumpDone = 0;   /* 是否已保存过RxBuf快照 */
static uint8_t s_dumpReq = 0;      /* 主循环打印请求 */
static uint8_t s_dumpBuf[64];      /* RxBuf前64字节快照 */
static uint8_t s_dumpTotal = 0;    /* 快照包的DMA长度字节 */

#if (RF_USB_SERIAL_ROLE == RF_SERIAL_ROLE_RX)
/* ---------------------------- 接收端 ---------------------------- */
rfipRx_t gRxParam;
__attribute__((__aligned__(4))) uint8_t RxBuf[264];  // 接收DMA buf不能小于264字节

/* 波特率配置包发送(接收端 -> 发送端) */
rfipTx_t gCfgTxParam;
__attribute__((__aligned__(4))) static uint8_t s_cfgTxBuf[TEST_PKT_HEAD_LEN + RF_CFG_LINE_CODING_LEN + 2];
static uint8_t s_cfgInfo[RF_CFG_LINE_CODING_LEN];   // 最近一次主机设置的line coding
static volatile uint8_t s_cfgReq;                   // 有待下发的配置
static volatile uint8_t s_cfgTxBusy;                // 配置包正在发送
static uint8_t s_cfgRetry;                          // 剩余发送次数
static uint16_t s_cfgDelay;                         // 重发延时(ms)
static uint8_t s_cfgSeq;                            // 配置包序号
static volatile uint8_t s_cfgResume;                // 需要恢复数据接收
static volatile uint8_t s_cfgSentReq;               // 有待打印的"已下发"记录
static uint32_t s_cfgSentBaud;                      // 最近一次下发的波特率

/* 数据包序列号去重 */
static uint8_t s_dataLastSeq;                       // 上一包收到的序号
static uint32_t s_dataDupCount;                     // 收到的重复包数
static uint32_t s_dataLostCount;                    // 检测到的丢包数
static uint32_t s_rxPktCount;                       // 有效包计数
static uint32_t s_rxBadCount;                       // 未找到合法帧头的次数
static uint32_t s_rxStatsAt;                        // 上次打印统计时的包计数
static volatile uint8_t s_dbgOff;                   // 诊断: 帧头偏移
static volatile uint8_t s_dbgLen;                   // 诊断: 数据长度
static volatile uint8_t s_dbgSeq;                   // 诊断: 序号
static volatile uint8_t s_dbgPending;               // 诊断: 待打印
static uint8_t s_dbgShown;                          // 诊断: 已打印条数
#else
/* ---------------------------- 发送端 ---------------------------- */
rfipTx_t gTxParam;
__attribute__((__aligned__(4))) uint8_t TxBuf[TEST_PKT_DATA_OFS + RF_USB_SERIAL_MAX_PAYLOAD + 1];

rfipRx_t gRxParam;
__attribute__((__aligned__(4))) uint8_t RxBuf[264];  // 接收DMA buf不能小于264字节

static volatile uint8_t s_txBusy;
static uint8_t s_txSeq;
static uint16_t s_txLastCount;                      // 上一轮UART字节数(用于聚合整帧)
static volatile uint8_t s_listening;                // 正在监听配置包
static volatile uint32_t s_baudApplied;            // 新波特率(供主循环打印)
static volatile uint32_t s_cfgBaud;                // 收到的配置包波特率(供主循环打印)
static volatile uint32_t s_cfgBadCnt;              // 配置包解析失败次数(供主循环打印)
static volatile uint32_t s_baudNew;                // 待生效的波特率
static volatile uint8_t s_baudReq;                 // 有待生效的波特率
static uint32_t s_baudCur = RF_USB_SERIAL_UART_BAUD;  // 当前生效的波特率
static volatile uint8_t s_listenFail;              // 监听启动失败返回码
static uint8_t s_listenFailShown;                  // 失败信息只打印一次
static volatile uint32_t s_crcErrCnt;              // 收到但CRC错误的包数
static uint8_t s_crcErrShown;                      // CRC错误信息只打印一次
#endif

/*********************************************************************
 * @fn      rf_frame_chk
 *
 * @brief   计算帧校验(异或): 覆盖包头 + 序号 + 数据
 *
 * @param   p   - 帧头指针(type,len,seq,data...)
 *          len - 参与校验的字节数(包头+数据)
 *
 * @return  校验值
 */
static uint8_t rf_frame_chk(const uint8_t *p, uint16_t len)
{
    uint8_t c = 0;
    uint16_t i;

    for(i = 0; i < len; i++)
    {
        c ^= p[i];
    }
    return c;
}

/*********************************************************************
 * @fn      fifo_put
 *
 * @brief   向环形缓冲区写入数据(缓冲区满时丢弃)
 *
 * @param   pDat - 数据指针
 *          len  - 数据长度
 *
 * @return  None.
 */
static void fifo_put(const uint8_t *pDat, uint16_t len)
{
    uint16_t i;
    uint16_t next;

    for(i = 0; i < len; i++)
    {
        next = (uint16_t)((s_fifoHead + 1) & SERIAL_FIFO_MASK);
        if(next == s_fifoTail)
        {
            return;
        }
        s_fifoBuf[s_fifoHead] = pDat[i];
        s_fifoHead = next;
    }
}

/*********************************************************************
 * @fn      fifo_get
 *
 * @brief   从环形缓冲区取出数据
 *
 * @param   pDat   - 数据指针
 *          maxLen - 本次最多取出的字节数
 *
 * @return  实际取出的字节数
 */
static uint16_t fifo_get(uint8_t *pDat, uint16_t maxLen)
{
    uint16_t len = 0;

    while((len < maxLen) && (s_fifoTail != s_fifoHead))
    {
        pDat[len++] = s_fifoBuf[s_fifoTail];
        s_fifoTail = (uint16_t)((s_fifoTail + 1) & SERIAL_FIFO_MASK);
    }
    return len;
}

/*********************************************************************
 * @fn      fifo_peek
 *
 * @brief   从环形缓冲区预读数据(不移动读指针)
 *
 * @param   pDat   - 数据指针
 *          maxLen - 本次最多读取的字节数
 *
 * @return  实际读到的字节数
 */
static uint16_t fifo_peek(uint8_t *pDat, uint16_t maxLen)
{
    uint16_t len = 0;
    uint16_t tail = s_fifoTail;

    while((len < maxLen) && (tail != s_fifoHead))
    {
        pDat[len++] = s_fifoBuf[tail];
        tail = (uint16_t)((tail + 1) & SERIAL_FIFO_MASK);
    }
    return len;
}

/*********************************************************************
 * @fn      fifo_consume
 *
 * @brief   提交已发送的数据, 移动读指针
 *
 * @param   len - 已成功发送的字节数
 *
 * @return  None.
 */
static void fifo_consume(uint16_t len)
{
    s_fifoTail = (uint16_t)((s_fifoTail + len) & SERIAL_FIFO_MASK);
}

/*********************************************************************
 * @fn      fifo_count
 *
 * @brief   获取环形缓冲中的有效字节数
 *
 * @return  字节数
 */
static uint16_t fifo_count(void)
{
    return (uint16_t)((s_fifoHead - s_fifoTail) & SERIAL_FIFO_MASK);
}

/*********************************************************************
 * @fn      app_usb_on_line_coding
 *
 * @brief   主机修改串口参数回调(USB中断中调用), 只保存参数并置标志
 *
 * @param   pCoding - 7字节line coding
 *
 * @return  None.
 */
void app_usb_on_line_coding(const uint8_t *pCoding)
{
#if (RF_USB_SERIAL_ROLE == RF_SERIAL_ROLE_RX)
    memcpy(s_cfgInfo, pCoding, RF_CFG_LINE_CODING_LEN);
    s_cfgReq = 1;
#else
    (void)pCoding;
#endif
}

/*********************************************************************
 * @fn      rf_dump_rxbuf
 *
 * @brief   打印RxBuf前若干字节, 并扫描所有可能的包头偏移,
 *          帮助确定DMA实际写入布局. 同时匹配数据包(0x55)和配置包(0x56)
 *
 * @param   None.
 *
 * @return  None.
 */
static void rf_dump_rxbuf(void)
{
    uint8_t i, off;
    uint8_t hit = 0xFF;

    PRINT("====== RX raw buf (first 32 bytes) ======\n");
    for(i = 0; i < 32; i++)
    {
        PRINT("%02X ", s_dumpBuf[i]);
        if((i & 0x0F) == 0x0F) PRINT("\n");
    }
    PRINT("dma len byte [1] = %d\n", s_dumpTotal);
    PRINT("------ offset scan ------\n");
    for(off = 0; off < RF_RX_HEAD_SCAN; off++)
    {
        uint8_t t = s_dumpBuf[off + 0];
        uint8_t l = s_dumpBuf[off + 1];
        uint8_t s = s_dumpBuf[off + 2];
        uint8_t ok = 0;

        if((l >= 2) && (l <= (RF_USB_SERIAL_MAX_PAYLOAD + 2)) &&
           ((uint16_t)(off + TEST_PKT_HEAD_LEN + l) <= sizeof(s_dumpBuf)))
        {
            ok = (rf_frame_chk(&s_dumpBuf[off], TEST_PKT_HEAD_LEN + l - 1) ==
                  s_dumpBuf[off + TEST_PKT_HEAD_LEN + l - 1]);
        }
        PRINT("  off[%d]: %02X %02X %02X  chk:%s\n",
              off, t, l, s, ok ? "OK <-- FRAME" : "NG");
        if((hit == 0xFF) && ((t == RF_USB_SERIAL_PKT_TYPE) || (t == RF_CFG_PKT_TYPE)) && ok)
        {
            hit = off;
        }
    }
    PRINT("==> header offset = %d\n", (hit == 0xFF) ? s_rxPktOffset : hit);
}

#if (RF_USB_SERIAL_ROLE == RF_SERIAL_ROLE_RX)

/*********************************************************************
 * @fn      rf_rx_start
 *
 * @brief   启动2.4G接收
 *
 * @param   None.
 *
 * @return  None.
 */
__HIGH_CODE
static void rf_rx_start(void)
{
    gRxParam.frequency = TEST_FREQUENCY;  // 配置接收的频点
    gRxParam.timeOut = 0;                 // 配置接收的超时时间，0则无超时
    RFIP_SetRx(&gRxParam);
}

/*********************************************************************
 * @fn      rf_rx_process_data
 *
 * @brief   解析接收到的数据包,将有效载荷写入环形缓冲区.
 *          - 校验type/length
 *          - 序列号去重/丢包检测
 *
 * @param   None.
 *
 * @return  None.
 */
__HIGH_CODE
static void rf_rx_process_data(void)
{
    uint8_t off;
    uint8_t len;
    uint8_t seq;
    uint8_t *pkt = NULL;

    /* 第一次收到包时保存快照, 由主循环打印(不能在中断里打印) */
    if(!s_rxDumpDone)
    {
        s_rxDumpDone = 1;
        memcpy(s_dumpBuf, RxBuf, sizeof(s_dumpBuf));
        s_dumpTotal = RxBuf[1];
        s_dumpReq = 1;
    }

    /*
     * 逐字节搜索合法帧:
     *   1) 基本模式数据从RxBuf[0]开始(包头2字节), 仍做小范围扫描以容忍偏移
     *   2) 帧尾带1字节异或校验, 校验通过才认为是有效帧
     */
    for(off = 0; off < RF_RX_HEAD_SCAN; off++)
    {
        uint8_t l = RxBuf[off + 1];

        if(RxBuf[off] != RF_USB_SERIAL_PKT_TYPE)
        {
            continue;
        }
        if((l < 2) || (l > (RF_USB_SERIAL_MAX_PAYLOAD + 2)))
        {
            continue;
        }
        if((uint16_t)(off + TEST_PKT_HEAD_LEN + l) > sizeof(RxBuf))
        {
            continue;
        }
        /* 校验不通过说明这段不是有效帧(长度读错/数据损坏), 直接丢弃 */
        if(rf_frame_chk(&RxBuf[off], TEST_PKT_HEAD_LEN + l - 1) !=
           RxBuf[off + TEST_PKT_HEAD_LEN + l - 1])
        {
            continue;
        }
        pkt = &RxBuf[off];
        break;
    }
    if(pkt == NULL)
    {
        s_rxBadCount++;
        return;
    }

    s_rxPktOffset = off;
    len  = (uint8_t)(pkt[1] - 2);   /* 实际数据长度(不含序号和校验字节) */
    seq  = pkt[2];

    /* 序列号去重: 与上一包序号相同, 判定为射频重发的重复包 */
    if((s_rxPktCount > 0) && (seq == s_dataLastSeq))
    {
        s_dataDupCount++;
        return;
    }

    /* 丢包检测 */
    if(s_rxPktCount > 0)
    {
        uint8_t expectNext = (uint8_t)(s_dataLastSeq + 1);
        if(seq > expectNext)
        {
            s_dataLostCount += (uint32_t)(seq - expectNext);
        }
    }
    s_dataLastSeq = seq;
    s_rxPktCount++;

    /* 前5包记录诊断信息, 由主循环打印(中断里不能打印) */
    if(s_dbgShown < 5)
    {
        s_dbgOff = off;
        s_dbgLen = len;
        s_dbgSeq = seq;
        s_dbgPending = 1;
    }

    /* 写入payload: 2字节包头 + 1字节序号之后 */
    fifo_put(pkt + TEST_PKT_DATA_OFS, len);
}

/*********************************************************************
 * @fn      rf_cfg_send
 *
 * @brief   把主机设置的line coding装入配置包并启动2.4G发送
 *
 * @param   None.
 *
 * @return  None.
 */
__HIGH_CODE
static void rf_cfg_send(void)
{
    s_cfgTxBuf[0] = RF_CFG_PKT_TYPE;                                   // 包头:类型
    s_cfgTxBuf[1] = (RF_CFG_LINE_CODING_LEN + 2);                      // 长度 = 序号1 + 数据7 + 校验1
    s_cfgTxBuf[2] = ++s_cfgSeq;                                        // 包头:序号
    memcpy(&s_cfgTxBuf[TEST_PKT_DATA_OFS], s_cfgInfo, RF_CFG_LINE_CODING_LEN);
    s_cfgTxBuf[TEST_PKT_DATA_OFS + RF_CFG_LINE_CODING_LEN] =
        rf_frame_chk(s_cfgTxBuf, TEST_PKT_DATA_OFS + RF_CFG_LINE_CODING_LEN);   // 帧尾校验

    /* 记录本次下发的波特率, 由主循环打印 */
    s_cfgSentBaud = (uint32_t)s_cfgInfo[0] | ((uint32_t)s_cfgInfo[1] << 8) |
                    ((uint32_t)s_cfgInfo[2] << 16) | ((uint32_t)s_cfgInfo[3] << 24);
    s_cfgSentReq = 1;

    s_cfgTxBusy = 1;
    RFIP_SetTxStart();
    mDelayuS(1);                            // 与官方例程一致: 启动后延时1us再配参数
    gCfgTxParam.frequency = TEST_FREQUENCY;
    gCfgTxParam.txDMA = (uint32_t)s_cfgTxBuf;
    RFIP_SetTxParm(&gCfgTxParam);
}

/*********************************************************************
 * @fn      rf_cfg_poll
 *
 * @brief   配置包发送/重发轮询(1ms调用一次)
 *
 * @param   None.
 *
 * @return  None.
 */
static void rf_cfg_poll(void)
{
    if(s_cfgTxBusy)
    {
        return;
    }
    if(s_cfgResume)                 // 发送结束后回到接收状态
    {
        s_cfgResume = 0;
        rf_rx_start();
        return;
    }
    if(s_cfgDelay)
    {
        s_cfgDelay--;
        return;
    }
    if(s_cfgReq)
    {
        s_cfgReq = 0;
        s_cfgRetry = RF_CFG_SEND_TIMES - 1;
        rf_cfg_send();
    }
    else if(s_cfgRetry)
    {
        s_cfgRetry--;
        rf_cfg_send();
    }
}

#else

/*********************************************************************
 * @fn      rf_tx_send
 *
 * @brief   从环形缓冲区取数据并启动2.4G发送, 硬件自动重发3次
 *
 * @param   None.
 *
 * @return  None.
 */
static void rf_tx_send(void)
{
    uint16_t len;

    len = fifo_get(&TxBuf[TEST_PKT_DATA_OFS], RF_USB_SERIAL_MAX_PAYLOAD);
    if(len == 0)
    {
        return;
    }
    TxBuf[0] = RF_USB_SERIAL_PKT_TYPE;               // 包头:类型
    TxBuf[1] = (uint8_t)(len + 2);                   // 长度 = 序号1 + 数据 + 校验1
    TxBuf[2] = ++s_txSeq;                            // 包头:序号(用于接收端去重)
    /* 帧尾校验: 覆盖2字节包头 + 序号 + 数据 */
    TxBuf[TEST_PKT_DATA_OFS + len] = rf_frame_chk(TxBuf, TEST_PKT_DATA_OFS + len);

    s_listening = 0;
    s_txBusy = 1;
    RFIP_SetTxStart();
    mDelayuS(1);                          // 与官方例程一致: 启动后延时1us再配参数
    gTxParam.frequency = TEST_FREQUENCY;  // 配置发送的频点
    gTxParam.txDMA = (uint32_t)TxBuf;
    RFIP_SetTxParm(&gTxParam);
}

/*********************************************************************
 * @fn      rf_listen_start
 *
 * @brief   启动2.4G监听(带超时), 用于接收接收端下发的波特率配置包
 *
 * @param   None.
 *
 * @return  None.
 */
__HIGH_CODE
static void rf_listen_start(void)
{
    bStatus_t st;

    /* 直接SetRx即可(与官方DTM例程一致), 反复Shut会把射频关掉导致收不到 */
    gRxParam.frequency = TEST_FREQUENCY;
    gRxParam.timeOut = 0;                  // 无超时: 一直监听, 发送前由rf_tx_send切换
    st = RFIP_SetRx(&gRxParam);
    s_listening = (st == 0) ? 1 : 0;        // 0 = 成功
    s_listenFail = (st == 0) ? 0 : (uint8_t)st;
}

/*********************************************************************
 * @fn      rf_cfg_rx_process
 *
 * @brief   解析配置包, 按主机设置的波特率重新配置UART1
 *
 * @param   None.
 *
 * @return  None.
 */
__HIGH_CODE
static void rf_cfg_rx_process(void)
{
    uint32_t baud;
    uint8_t off;
    uint8_t *pkt = NULL;
    uint8_t *p;

    /* TX端首次收到包时保存快照, 由主循环打印 */
    if(!s_rxDumpDone)
    {
        s_rxDumpDone = 1;
        memcpy(s_dumpBuf, RxBuf, sizeof(s_dumpBuf));
        s_dumpTotal = RxBuf[1];
        s_dumpReq = 1;
    }

    /* 同接收端: 搜索合法配置包帧(校验通过才算) */
    for(off = 0; off < RF_RX_HEAD_SCAN; off++)
    {
        uint8_t l = RxBuf[off + 1];
        uint8_t dataLen;

        if(RxBuf[off] != RF_CFG_PKT_TYPE)
        {
            continue;
        }
        if(l < (RF_CFG_LINE_CODING_LEN + 2))     /* 序号 + 数据 + 校验 */
        {
            continue;
        }
        dataLen = (uint8_t)(l - 2);
        if(dataLen != RF_CFG_LINE_CODING_LEN)
        {
            continue;
        }
        if((uint16_t)(off + TEST_PKT_HEAD_LEN + l) > sizeof(RxBuf))
        {
            continue;
        }
        if(rf_frame_chk(&RxBuf[off], TEST_PKT_HEAD_LEN + l - 1) !=
           RxBuf[off + TEST_PKT_HEAD_LEN + l - 1])
        {
            continue;
        }
        pkt = &RxBuf[off];
        break;
    }
    if(pkt == NULL)
    {
        s_cfgBadCnt++;      // 收到包但不是合法配置包(长度/校验不符)
        return;
    }
    s_rxPktOffset = off;

    p = pkt + TEST_PKT_DATA_OFS;   /* payload 起点(跳过类型/长度/序号) */
    baud = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    if((baud < RF_CFG_BAUD_MIN) || (baud > RF_CFG_BAUD_MAX))
    {
        return;
    }
    s_cfgBaud = baud;   /* 交给主循环打印, 中断里不改UART配置 */
    if(baud != s_baudCur)
    {
        s_baudNew = baud;
        s_baudReq = 1;   /* 由主循环在空闲时重新初始化UART1 */
    }
}

/*********************************************************************
 * @fn      rf_tx_poll
 *
 * @brief   发送端1ms轮询: 聚合UART数据, 有数据发数据, 无数据监听配置包
 *
 * @param   None.
 *
 * @return  None.
 */
static void rf_tx_poll(void)
{
    if(s_txBusy)
    {
        return;
    }
    /* 聚合: 攒满一包立即发送; 未满则等UART安静1ms后再发, 避免把一帧数据拆成多个小包 */
    {
        uint16_t count;
        count = fifo_count();
        if(count >= RF_USB_SERIAL_MAX_PAYLOAD)
        {
            s_txLastCount = 0;
            rf_tx_send();
        }
        else if(count > 0)
        {
            if(count == s_txLastCount)      // 连续1ms没有新字节, 认为这一帧收完了
            {
                s_txLastCount = 0;
                rf_tx_send();
            }
            else
            {
                s_txLastCount = count;      // 继续等待
            }
        }
        else
        {
            s_txLastCount = 0;
            if(!s_listening)
            {
                rf_listen_start();
            }
        }
    }
}

/*********************************************************************
 * @fn      rf_uart_init
 *
 * @brief   发送端UART1初始化(PA8-RX, PA9-TX)
 *
 * @param   None.
 *
 * @return  None.
 */
static void rf_uart_init(void)
{
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);

    UART1_DefInit();
    UART1_BaudRateCfg(RF_USB_SERIAL_UART_BAUD);
    UART1_ByteTrigCfg(UART_1BYTE_TRIG);
    UART1_INTCfg(ENABLE, RB_IER_RECV_RDY | RB_IER_LINE_STAT);
    PFIC_EnableIRQ(UART1_IRQn);
}

/*********************************************************************
 * @fn      rf_uart_set_baud
 *
 * @brief   运行中修改UART1波特率: 先关中断/清FIFO, 再重设分频并重新使能
 *
 * @param   baud - 新波特率
 *
 * @return  None.
 */
static void rf_uart_set_baud(uint32_t baud)
{
    UART1_INTCfg(DISABLE, RB_IER_RECV_RDY | RB_IER_LINE_STAT);
    R8_UART1_IER = 0;                                   // 关闭收发
    R8_UART1_FCR = RB_FCR_TX_FIFO_CLR | RB_FCR_RX_FIFO_CLR | RB_FCR_FIFO_EN;

    UART1_BaudRateCfg(baud);                            // 重设波特率分频
    R8_UART1_DIV = 1;                                   // 串口时钟不分频
    R8_UART1_LCR = RB_LCR_WORD_SZ;                      // 8位数据

    UART1_ByteTrigCfg(UART_1BYTE_TRIG);                 // 1字节触发
    R8_UART1_IER = RB_IER_TXD_EN;                       // 使能发送
    UART1_INTCfg(ENABLE, RB_IER_RECV_RDY | RB_IER_LINE_STAT);

    s_fifoHead = 0;                                     // 丢弃旧波特率下的残留数据
    s_fifoTail = 0;
    s_baudCur = baud;
    s_baudApplied = baud;
}

/*********************************************************************
 * @fn      UART1_IRQHandler
 *
 * @brief   UART1中断处理,接收的数据写入环形缓冲区
 *
 * @param   None.
 *
 * @return  None.
 */
__INTERRUPT
__HIGH_CODE
void UART1_IRQHandler(void)
{
    uint8_t buf[8];
    uint16_t len;

    switch(UART1_GetITFlag())
    {
        case UART_II_RECV_RDY:
        case UART_II_RECV_TOUT:
            len = UART1_RecvString(buf);
            if(len)
            {
                fifo_put(buf, len);
            }
            break;

        case UART_II_LINE_STAT:
            UART1_GetLinSTA();
            break;

        default:
            break;
    }
}

#endif

/*********************************************************************
 * @fn      RF_ProcessCallBack
 *
 * @brief   rf中断处理程序
 *
 * @param   sta - 中断状态.
 *          id  - 保留
 *
 * @return  None.
 */
__HIGH_CODE
void RF_ProcessCallBack(rfRole_States_t sta, uint8_t id)
{
    (void)id;

#if (RF_USB_SERIAL_ROLE == RF_SERIAL_ROLE_RX)
    if(sta & RF_STATE_RX)
    {
        rf_rx_process_data();
        rf_rx_start();
    }
    if(sta & (RF_STATE_RX_CRCERR | RF_STATE_TIMEOUT))
    {
        rf_rx_start();
    }
    if(sta & RF_STATE_TX_FINISH)   // 配置包发送结束
    {
        s_cfgTxBusy = 0;
        if(s_cfgRetry)
        {
            s_cfgDelay = RF_CFG_RETRY_MS;   // 延时后重发
        }
        s_cfgResume = 1;                // 由轮询恢复数据接收
    }
#else
    if(sta & RF_STATE_TX_FINISH)
    {
        s_txBusy = 0;
        if(fifo_count() == 0)          // 没有待发数据时立刻转监听, 不留1ms空窗
        {
            rf_listen_start();
        }
    }
    if(sta & RF_STATE_RX)              // 收到配置包
    {
        rf_cfg_rx_process();
        rf_listen_start();             // 处理完继续监听, 配置包是连续重发的
    }
    if(sta & (RF_STATE_RX_CRCERR | RF_STATE_TIMEOUT))
    {
        if(sta & RF_STATE_RX_CRCERR)
        {
            s_crcErrCnt++;             // 收到了包但CRC错(参数不一致/误码)
        }
        rf_listen_start();             // 超时/校验错后立即重新监听
    }
#endif
}

/*********************************************************************
 * @fn      RF_Serial_Process
 *
 * @brief   无线串口数据处理(主循环调用):
 *          接收端将缓冲数据通过USB CDC上传给主机
 *          打印丢包/重复统计信息
 *
 * @param   None.
 *
 * @return  None.
 */
void RF_Serial_Process(void)
{
    /* 首次收到RF包后, 在这里(非中断上下文)打印原始缓冲, 便于确认帧头偏移 */
    if(s_dumpReq)
    {
        s_dumpReq = 0;
        rf_dump_rxbuf();
    }
#if (RF_USB_SERIAL_ROLE == RF_SERIAL_ROLE_RX)
    {
        uint8_t buf[64];
        uint16_t len;

        if(app_usb_cfg_ready() && !app_usb_in_busy())
        {
            /* 先预读, 发送成功后才提交, 避免USB忙时丢数据 */
            len = fifo_peek(buf, sizeof(buf));
            if(len && app_usb_send(buf, len))
            {
                fifo_consume(len);
            }
        }

        /* 主机改波特率后, 这里确认配置包已经下发 */
        if(s_cfgSentReq)
        {
            s_cfgSentReq = 0;
            PRINT("cfg send: baud=%d\n", (int)s_cfgSentBaud);
        }

        /* 每100个有效包打印一次统计, 便于判断丢包/重传是否生效 */
        if(s_rxPktCount && (s_rxPktCount % 100 == 0))
        {
            if(s_rxStatsAt != s_rxPktCount)
            {
                s_rxStatsAt = s_rxPktCount;
                PRINT("rx stat: pkt=%d dup=%d lost=%d bad=%d off=%d\n",
                      (int)s_rxPktCount, (int)s_dataDupCount,
                      (int)s_dataLostCount, (int)s_rxBadCount, s_rxPktOffset);
            }
        }

        /* 前5包诊断: 打印帧头偏移/长度/序号 */
        if(s_dbgPending)
        {
            s_dbgPending = 0;
            s_dbgShown++;
            PRINT("rx pkt%d: off=%d len=%d seq=%d\n",
                  s_dbgShown, s_dbgOff, s_dbgLen, s_dbgSeq);
        }
    }
#else
    /* 收到新波特率: 在UART空闲时重新初始化(先关中断/清FIFO/重设分频) */
    if(s_baudReq && !s_txBusy)
    {
        s_baudReq = 0;
        rf_uart_set_baud(s_baudNew);
    }

    /* 监听启动失败(打印一次, 恢复后复位) */
    if(s_listenFail)
    {
        if(!s_listenFailShown)
        {
            s_listenFailShown = 1;
            PRINT("listen fail: st=%d\n", s_listenFail);
        }
    }
    else
    {
        s_listenFailShown = 0;
    }

    /* 收到包但CRC错误: 说明在接收, 但2.4G参数不一致 (打印一次) */
    if(s_crcErrCnt)
    {
        if(!s_crcErrShown)
        {
            s_crcErrShown = 1;
            PRINT("rf crcerr: %d\n", (int)s_crcErrCnt);
        }
    }
    else
    {
        s_crcErrShown = 0;
    }

    if(s_cfgBadCnt)   // 收到了2.4G包, 但不是合法配置包
    {
        PRINT("cfg rx bad: %d\n", (int)s_cfgBadCnt);
        s_cfgBadCnt = 0;
    }
    if(s_cfgBaud)   // 收到的配置包波特率
    {
        PRINT("cfg rx: offset=%d baud=%d\n", s_rxPktOffset, (int)s_cfgBaud);
        s_cfgBaud = 0;
    }
    if(s_baudApplied)   // 主机下发的波特率已生效
    {
        PRINT("uart1 baud set: %d (DL=%d)\n", (int)s_baudApplied, (int)R16_UART1_DL);
        s_baudApplied = 0;
    }
#endif
}

/*********************************************************************
 * @fn      RFRole_ProcessEvent
 *
 * @brief   RF层系统任务处理
 *
 * @param   task_id - 任务ID
 *          events  - 事件
 *
 * @return  未处理的事件
 */
tmosEvents RFRole_ProcessEvent(tmosTaskID task_id, tmosEvents events)
{
    if(events & SYS_EVENT_MSG)
    {
        uint8_t *msgPtr;

        msgPtr = tmos_msg_receive(task_id);
        if(msgPtr)
        {
            /* De-allocate */
            tmos_msg_deallocate(msgPtr);
        }
        return events ^ SYS_EVENT_MSG;
    }
#if (RF_USB_SERIAL_ROLE == RF_SERIAL_ROLE_RX)
    if(events & RF_START_RX_EVENT)
    {
        rf_rx_start();
        return events ^ RF_START_RX_EVENT;
    }
    if(events & RF_CFG_POLL_EVENT)
    {
        rf_cfg_poll();
        return events ^ RF_CFG_POLL_EVENT;
    }
#else
    if(events & RF_TX_POLL_EVENT)
    {
        rf_tx_poll();
        return events ^ RF_TX_POLL_EVENT;
    }
#endif
    return 0;
}

/*********************************************************************
 * @fn      RFRole_Init
 *
 * @brief   RF应用层初始化
 *
 * @param   None.
 *
 * @return  None.
 */
void RFRole_Init(void)
{
    rfTaskID = TMOS_ProcessEventRegister(RFRole_ProcessEvent);
    {
        rfRoleConfig_t conf = {0};

        conf.TxPower = LL_TX_PWR_4_DBM;
        conf.rfProcessCB = RF_ProcessCallBack;
        /* RX角色也要监听RF_STATE_TX_FINISH(发配置包后恢复接收) */
        conf.processMask = RF_STATE_RX | RF_STATE_RX_CRCERR | RF_STATE_TIMEOUT | RF_STATE_TX_FINISH;
        RFRole_BasicInit(&conf);
    }
    {
        gParm.accessAddress = TEST_ACCESS_ADDRESS;
        gParm.crcInit = TEST_CRC_INIT;
        /* PHY 2Mbps, 关闭ACK(Basic模式不支持) */
        gParm.properties = LLE_MODE_PHY_2M;
        /* 重传间隔: 硬件自动重发时两次发送之间的间隔 */
        gParm.sendInterval = RF_TX_RETRY_INTERVAL;
        /* 发送稳定时间: 切换到TX后需要等待射频稳定的时间 */
        gParm.sendTime = 20 * 2;
        /* 最大接收长度 = 2字节包头 + 1字节序号 + 最大payload + 1字节校验 */
        gParm.rxMaxLen = TEST_PKT_DATA_OFS + RF_USB_SERIAL_MAX_PAYLOAD + 1;
        RFRole_SetParam(&gParm);
    }

#if (RF_USB_SERIAL_ROLE == RF_SERIAL_ROLE_RX)
    // RX相关参数
    {
        gRxParam.accessAddress = gParm.accessAddress;
        gRxParam.crcInit = gParm.crcInit;
        gRxParam.properties = gParm.properties;
        gRxParam.rxDMA = (uint32_t)RxBuf;
        gRxParam.rxMaxLen = TEST_PKT_DATA_OFS + RF_USB_SERIAL_MAX_PAYLOAD + 1;
    }
    // 配置包发送参数(硬件也自动重发3次)
    {
        gCfgTxParam.accessAddress = gParm.accessAddress;
        gCfgTxParam.crcInit = gParm.crcInit;
        gCfgTxParam.properties = gParm.properties;
        gCfgTxParam.txDMA = (uint32_t)s_cfgTxBuf;
        gCfgTxParam.sendCount = RF_TX_RETRY_COUNT;    // 配置包也重发
        gCfgTxParam.sendTime = gParm.sendTime;
    }
#else
    // TX相关参数(数据包重发3次)
    {
        gTxParam.accessAddress = gParm.accessAddress;
        gTxParam.crcInit = gParm.crcInit;
        gTxParam.properties = gParm.properties;
        gTxParam.txDMA = (uint32_t)TxBuf;
        gTxParam.sendCount = RF_TX_RETRY_COUNT;      // 每包发3次
        gTxParam.sendTime = gParm.sendTime;
    }
    // 监听配置包参数
    {
        gRxParam.accessAddress = gParm.accessAddress;
        gRxParam.crcInit = gParm.crcInit;
        gRxParam.properties = gParm.properties;
        gRxParam.rxDMA = (uint32_t)RxBuf;
        gRxParam.rxMaxLen = TEST_PKT_DATA_OFS + RF_USB_SERIAL_MAX_PAYLOAD + 1;
    }
#endif

    PFIC_EnableIRQ(BLEB_IRQn);
    PFIC_EnableIRQ(BLEL_IRQn);
    PRINT("rf role init.id=%d\n", rfTaskID);
    PRINT("rf param: freq=%d AA=%08X CRC=%06X PHY=2M retry=%d\n",
          TEST_FREQUENCY, TEST_ACCESS_ADDRESS, TEST_CRC_INIT, RF_TX_RETRY_COUNT);

#if (RF_USB_SERIAL_ROLE == RF_SERIAL_ROLE_RX)
    PRINT("----------------- usb wireless serial: rx -----------------\n");
    s_fifoHead = 0;
    s_fifoTail = 0;
    s_cfgReq = 0;
    s_cfgTxBusy = 0;
    s_cfgRetry = 0;
    s_cfgDelay = 0;
    s_cfgSeq = 0;
    s_cfgResume = 0;
    s_dataLastSeq = 0;
    s_dataDupCount = 0;
    s_dataLostCount = 0;
    s_rxPktCount = 0;
    s_rxBadCount = 0;
    s_rxStatsAt = 0;
    s_dbgPending = 0;
    s_dbgShown = 0;
    s_rxDumpDone = 0;
    s_dumpReq = 0;
    s_rxPktOffset = 0;
    tmos_set_event(rfTaskID, RF_START_RX_EVENT);
    // 1ms轮询一次, 处理主机下发的波特率配置
    tmos_start_reload_task(rfTaskID, RF_CFG_POLL_EVENT, MS1_TO_SYSTEM_TIME(1));
#else
    PRINT("----------------- usb wireless serial: tx -----------------\n");
    rf_uart_init();
    PRINT("uart1 init ok\n");
    s_rxPktOffset = 0;
    s_rxDumpDone = 0;
    s_dumpReq = 0;
    s_txSeq = 0;
    s_txBusy = 0;
    s_txLastCount = 0;
    s_listening = 0;
    s_baudApplied = 0;
    s_cfgBaud = 0;
    /* 启动后立刻进入监听, 不要等1ms轮询 */
    {
        bStatus_t st;
        gRxParam.frequency = TEST_FREQUENCY;
        gRxParam.timeOut = 0;
        st = RFIP_SetRx(&gRxParam);
        s_listening = (st == 0) ? 1 : 0;
        s_listenFail = (st == 0) ? 0 : (uint8_t)st;
        PRINT("rf_listen_start: st=%d listening=%d\n", st, s_listening);
    }
    // 1ms轮询一次,聚合UART数据后按包发送,空闲时监听配置包
    tmos_start_reload_task(rfTaskID, RF_TX_POLL_EVENT, MS1_TO_SYSTEM_TIME(1));
    PRINT("rf tx poll started, task=%d\n", rfTaskID);
#endif
}

/******************************** endfile @rf ******************************/
