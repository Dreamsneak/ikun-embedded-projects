/********************************** (C) COPYRIGHT *******************************
 * File Name          : app_usb.c
 * Author             : 混分巨兽龙某某
 * Version            : V1.0
 * Date               : 2026/11/07
 * Description        : CH585 USB CDC(虚拟串口)设备实现
 *
 *                      端点分配:
 *                      端点0(EP0)      : 控制端点
 *                      端点1(EP1 OUT)  : 主机下发数据(0x01)
 *                      端点1(EP1 IN)   : 设备上传数据(0x81)
 *                      端点2(EP2 IN)   : CDC串口状态通知(0x82,未使用)
 *
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

/******************************************************************************/
/* 头文件包含 */
#include "CONFIG.h"
#include <string.h>
#include "app_usb.h"

/*********************************************************************
 * MACROS
 */
#define DevEP0SIZE                  0x40

/* CDC类请求 */
#define CDC_SET_LINE_CODING         0x20
#define CDC_GET_LINE_CODING         0x21
#define CDC_SET_CONTROL_LINE_STATE  0x22

/*********************************************************************
 * GLOBAL TYPEDEFS
 */
/* 设备描述符 */
static const uint8_t MyDevDescr[] = {
    0x12, 0x01, 0x10, 0x01, 0x02, 0x00, 0x00, DevEP0SIZE,
    0x86, 0x1A, 0x40, 0x80, 0x00, 0x30,
    0x01, 0x02, 0x03, 0x01
};

/* 配置描述符(CDC-ACM, 共67字节) */
static const uint8_t MyCfgDescr[] = {
    0x09, 0x02, 0x43, 0x00, 0x02, 0x01, 0x00, 0x80, 0x30,
    /* 接口0: CDC控制接口 */
    0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,
    0x05, 0x24, 0x00, 0x10, 0x01,
    0x04, 0x24, 0x02, 0x02,
    0x05, 0x24, 0x06, 0x00, 0x01,
    0x05, 0x24, 0x01, 0x01, 0x00,
    /* 中断上传端点: EP2 IN */
    0x07, 0x05, 0x82, 0x03, 0x08, 0x00, 0x01,
    /* 接口1: CDC数据接口 */
    0x09, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
    0x07, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00,
    0x07, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00,
};

/* 语言ID描述符 */
static const uint8_t MyLangDescr[] = {0x04, 0x03, 0x09, 0x04};

/* 字符串描述符, 初始化时由字符串生成 */
static uint8_t MyManuStr[2 + 2 * 16];
static uint8_t MyProdStr[2 + 2 * 32];
static uint8_t MySerStr[2 + 2 * 16];

/* 端点 RAM 缓冲区 */
__attribute__((aligned(4))) uint8_t EP0_Databuf[64 + 64 + 64];
__attribute__((aligned(4))) uint8_t EP1_Databuf[64 + 64];
__attribute__((aligned(4))) uint8_t EP2_Databuf[64 + 64];
__attribute__((aligned(4))) uint8_t EP3_Databuf[64 + 64];

static uint8_t DevConfig;
static uint8_t SetupReqCode;
static uint16_t SetupReqLen;
static const uint8_t *pDescr;

/* CDC串口参数: 115200, 1停止位, 无校验, 8数据位 */
static uint8_t LineCoding[7] = {0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08};

/*********************************************************************
 * @fn      usb_build_str_desc
 *
 * @brief   根据ASCII字符串生成USB字符串描述符(UTF-16LE)
 *
 * @param   pDst - 描述符缓冲区
 *          pStr - 字符串
 *
 * @return  描述符指针
 */
static uint8_t *usb_build_str_desc(uint8_t *pDst, const char *pStr)
{
    uint8_t i = 0;
    uint8_t j;

    while(pStr[i] && (i < 31))
    {
        i++;
    }
    pDst[0] = (uint8_t)(2 * i + 2);
    pDst[1] = 0x03;
    for(j = 0; j < i; j++)
    {
        pDst[2 + 2 * j] = (uint8_t)pStr[j];
        pDst[3 + 2 * j] = 0;
    }
    return pDst;
}

/*********************************************************************
 * @fn      usb_ep1_out_handler
 *
 * @brief   主机下发数据处理(发送端角色可将此数据转发至2.4G)
 *
 * @param   pDat - 数据指针
 *          len  - 数据长度
 *
 * @return  None.
 */
static void usb_ep1_out_handler(const uint8_t *pDat, uint16_t len)
{
    (void)pDat;
    (void)len;
}

/*********************************************************************
 * @fn      app_usb_init
 *
 * @brief   USB CDC设备初始化
 *
 * @param   None.
 *
 * @return  None.
 */
void app_usb_init(void)
{
    usb_build_str_desc(MyManuStr, "wch.cn");
    usb_build_str_desc(MyProdStr, "CH585 Wireless Serial");
    usb_build_str_desc(MySerStr, "20260101");

    pEP0_RAM_Addr = EP0_Databuf;
    pEP1_RAM_Addr = EP1_Databuf;
    pEP2_RAM_Addr = EP2_Databuf;
    pEP3_RAM_Addr = EP3_Databuf;

    DevConfig = 0;

    USB_DeviceInit();
    PFIC_EnableIRQ(USB_IRQn);
}

/*********************************************************************
 * @fn      app_usb_cfg_ready
 *
 * @brief   主机是否已完成配置
 *
 * @param   None.
 *
 * @return  1 - 已配置; 0 - 未配置
 */
uint8_t app_usb_cfg_ready(void)
{
    return DevConfig;
}

/*********************************************************************
 * @fn      app_usb_in_busy
 *
 * @brief   端点1 IN是否正在传输
 *
 * @param   None.
 *
 * @return  1 - 忙; 0 - 空闲
 */
uint8_t app_usb_in_busy(void)
{
    return ((R8_UEP1_CTRL & MASK_UEP_T_RES) == UEP_T_RES_ACK) ? 1 : 0;
}

/*********************************************************************
 * @fn      app_usb_send
 *
 * @brief   通过CDC虚拟串口上传数据
 *
 * @param   pDat - 数据指针
 *          len  - 数据长度(最大64字节)
 *
 * @return  1 - 已启动传输; 0 - 端点忙或长度为0
 */
uint8_t app_usb_send(const uint8_t *pDat, uint16_t len)
{
    if((len == 0) || app_usb_in_busy())
    {
        return 0;
    }
    if(len > DevEP0SIZE)
    {
        len = DevEP0SIZE;
    }
    memcpy(pEP1_IN_DataBuf, pDat, len);
    DevEP1_IN_Deal((uint8_t)len);
    return 1;
}

/*********************************************************************
 * @fn      USB_DevTransProcess
 *
 * @brief   USB设备传输处理
 *
 * @param   None.
 *
 * @return  None.
 */
__HIGH_CODE
void USB_DevTransProcess(void)
{
    uint8_t len, chtype, intflag, errflag;

    intflag = R8_USB_INT_FG;

    if(intflag & RB_UIF_TRANSFER)
    {
        if((R8_USB_INT_ST & MASK_UIS_TOKEN) != MASK_UIS_TOKEN)
        {
            switch(R8_USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP))
            {
                case UIS_TOKEN_IN: /* 端点0 IN: 描述符后续分片 */
                    if(SetupReqCode == USB_GET_DESCRIPTOR)
                    {
                        len = (SetupReqLen >= DevEP0SIZE) ? DevEP0SIZE : (uint8_t)SetupReqLen;
                        if(len)
                        {
                            memcpy(pEP0_DataBuf, pDescr, len);
                            SetupReqLen -= len;
                            pDescr += len;
                        }
                        R8_UEP0_T_LEN = len;
                        R8_UEP0_CTRL ^= RB_UEP_T_TOG;
                    }
                    else if(SetupReqCode == USB_SET_ADDRESS)
                    {
                        R8_USB_DEV_AD = (uint8_t)((R8_USB_DEV_AD & RB_UDA_GP_BIT) | SetupReqLen);
                        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                    }
                    else
                    {
                        R8_UEP0_T_LEN = 0;
                        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                    }
                    break;

                case UIS_TOKEN_OUT: /* 端点0 OUT: SET_LINE_CODING数据阶段 */
                    len = R8_USB_RX_LEN;
                    if(len && (SetupReqCode == CDC_SET_LINE_CODING) && (len >= 7))
                    {
                        memcpy(LineCoding, pEP0_DataBuf, 7);
                        app_usb_on_line_coding(LineCoding);   /* 通知应用层下发新波特率 */
                    }
                    R8_UEP0_T_LEN = 0;
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_ACK;
                    break;

                case UIS_TOKEN_OUT | 1: /* 端点1 OUT: 主机下发数据 */
                    if(R8_USB_INT_ST & RB_UIS_TOG_OK)
                    {
                        usb_ep1_out_handler(pEP1_OUT_DataBuf, R8_USB_RX_LEN);
                    }
                    break;

                case UIS_TOKEN_IN | 1: /* 端点1 IN: 上传完成 */
                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    break;

                case UIS_TOKEN_OUT | 2: /* 端点2 OUT: 未使用 */
                    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_R_RES) | UEP_R_RES_ACK;
                    break;

                case UIS_TOKEN_IN | 2: /* 端点2 IN: 未使用 */
                    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    break;

                default:
                    break;
            }
            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }

        if(R8_USB_INT_ST & RB_UIS_SETUP_ACT)
        {
            R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
            SetupReqLen = pSetupReqPak->wLength;
            SetupReqCode = pSetupReqPak->bRequest;
            chtype = pSetupReqPak->bRequestType;
            len = 0;
            errflag = 0;

            if((chtype & USB_REQ_TYP_MASK) == USB_REQ_TYP_STANDARD)
            {
                switch(SetupReqCode)
                {
                    case USB_GET_DESCRIPTOR:
                        switch(pSetupReqPak->wValue >> 8)
                        {
                            case USB_DESCR_TYP_DEVICE:
                                pDescr = MyDevDescr;
                                len = sizeof(MyDevDescr);
                                break;
                            case USB_DESCR_TYP_CONFIG:
                                pDescr = MyCfgDescr;
                                len = sizeof(MyCfgDescr);
                                break;
                            case USB_DESCR_TYP_STRING:
                                switch(pSetupReqPak->wValue & 0xFF)
                                {
                                    case 0:
                                        pDescr = MyLangDescr;
                                        len = MyLangDescr[0];
                                        break;
                                    case 1:
                                        pDescr = MyManuStr;
                                        len = MyManuStr[0];
                                        break;
                                    case 2:
                                        pDescr = MyProdStr;
                                        len = MyProdStr[0];
                                        break;
                                    case 3:
                                        pDescr = MySerStr;
                                        len = MySerStr[0];
                                        break;
                                    default:
                                        errflag = 0xFF;
                                        break;
                                }
                                break;
                            default:
                                errflag = 0xFF;
                                break;
                        }
                        if(!errflag)
                        {
                            if(SetupReqLen > len)
                            {
                                SetupReqLen = len;
                            }
                            len = (uint8_t)((SetupReqLen >= DevEP0SIZE) ? DevEP0SIZE : SetupReqLen);
                            if(len)
                            {
                                memcpy(pEP0_DataBuf, pDescr, len);
                            }
                            pDescr += len;
                        }
                        break;

                    case USB_SET_ADDRESS:
                        SetupReqLen = (uint16_t)(pSetupReqPak->wValue & 0xFF);
                        len = 0;
                        break;

                    case USB_GET_CONFIGURATION:
                        pEP0_DataBuf[0] = DevConfig;
                        if(SetupReqLen > 1)
                        {
                            SetupReqLen = 1;
                        }
                        break;

                    case USB_SET_CONFIGURATION:
                        DevConfig = (uint8_t)(pSetupReqPak->wValue & 0xFF);
                        break;

                    case USB_GET_INTERFACE:
                        pEP0_DataBuf[0] = 0;
                        SetupReqLen = 1;
                        break;

                    case USB_GET_STATUS:
                        pEP0_DataBuf[0] = 0;
                        pEP0_DataBuf[1] = 0;
                        SetupReqLen = 2;
                        break;

                    case USB_CLEAR_FEATURE:
                        if((chtype & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP)
                        {
                            switch(pSetupReqPak->wIndex & 0xFF)
                            {
                                case 0x81:
                                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK;
                                    break;
                                case 0x01:
                                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_ACK;
                                    break;
                                case 0x82:
                                    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK;
                                    break;
                                case 0x02:
                                    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_ACK;
                                    break;
                                default:
                                    errflag = 0xFF;
                                    break;
                            }
                        }
                        else
                        {
                            errflag = 0xFF;
                        }
                        break;

                    default:
                        errflag = 0xFF;
                        break;
                }
            }
            else if((chtype & USB_REQ_TYP_MASK) == USB_REQ_TYP_CLASS)
            {
                switch(SetupReqCode)
                {
                    case CDC_SET_LINE_CODING: /* 数据阶段由端点0 OUT处理 */
                        break;

                    case CDC_GET_LINE_CODING:
                        memcpy(pEP0_DataBuf, LineCoding, sizeof(LineCoding));
                        if(SetupReqLen > (uint16_t)sizeof(LineCoding))
                        {
                            SetupReqLen = (uint16_t)sizeof(LineCoding);
                        }
                        break;

                    case CDC_SET_CONTROL_LINE_STATE:
                        SetupReqLen = 0;
                        break;

                    default:
                        errflag = 0xFF;
                        break;
                }
            }
            else
            {
                errflag = 0xFF;
            }

            if(errflag == 0xFF)
            {
                R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL;
            }
            else
            {
                if(chtype & 0x80)
                {
                    len = (uint8_t)((SetupReqLen >= DevEP0SIZE) ? DevEP0SIZE : SetupReqLen);
                    SetupReqLen -= len;
                    R8_UEP0_T_LEN = len;
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
                }
                else
                {
                    R8_UEP0_T_LEN = 0;
                    if(SetupReqLen)
                    {
                        R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
                    }
                    else
                    {
                        R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_ACK;
                    }
                }
            }
            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }
    }
    else if(intflag & RB_UIF_BUS_RST)
    {
        R8_USB_DEV_AD = 0;
        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
        R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
        DevConfig = 0;
        R8_USB_INT_FG = RB_UIF_BUS_RST;
    }
    else if(intflag & RB_UIF_SUSPEND)
    {
        R8_USB_INT_FG = RB_UIF_SUSPEND;
    }
    else
    {
        R8_USB_INT_FG = intflag;
    }
}

/*********************************************************************
 * @fn      USB_IRQHandler
 *
 * @brief   USB中断处理
 *
 * @param   None.
 *
 * @return  None.
 */
__INTERRUPT
__HIGH_CODE
void USB_IRQHandler(void)
{
    USB_DevTransProcess();
}

/******************************** endfile @ app_usb ******************************/
