/*
 * @Description:
 * @Author: hecai
 * @Date: 2021-11-02 10:00:47
 * @LastEditTime: 2022-12-10 23:11:42
 * @FilePath: \ls_ble_sdk\dev\project\ble\ble_uart_server\parser.c
 */
#include "parser.h"
#include "Tools.h"
#include "lscrypt.h"
#include "log.h"
#include <string.h>

u8 buff[64];
u8 tmpBuff[64];
SYS_STATE curState = SYS_IDEL;
u8 lastTime[11] = {0};
u8 pw[20] = {0};

u32 cbc_ecb_key_128[] = {
    0x45344E49,
    0x764F734C,
    0x36415769,
    0x4678346B};
/**
 * @description:
 * @param {*}
 * @return {*}
 */
void crypt_init(void)
{
    HAL_LSCRYPT_Init();
    HAL_LSCRYPT_AES_Key_Config(cbc_ecb_key_128, AES_KEY_128);
}

void crypt_deinit(void)
{
    HAL_LSCRYPT_DeInit();
}
/**
 * @description: 解码数据
 * @param {u8 *} dat
 * @param {u8} len 长度为16的整数倍，最大不超过64
 * @return {*}
 */
void decodeInfo(const u8 *dat, u8 len)
{
    u8 l, i, j;
    crypt_init();
    l = len / 16;
    memset(buff, '\0', 64);
    for (i = 0; i < l; i++)
    {
        for (j = 0; j < 16; j++)
        {
            buff[i * 16 + j] = dat[i * 16 + 15 - j];
        }
    }
    HAL_LSCRYPT_AES_ECB_Decrypt(buff, len, tmpBuff);
    for (i = 0; i < l; i++)
    {
        for (j = 0; j < 16; j++)
        {
            buff[i * 16 + j] = tmpBuff[i * 16 + 15 - j];
        }
    }
    crypt_deinit();
}

void SetSysState(SYS_STATE state)
{
    curState = state;
}
// /**
//  * @description: 编码数据,空位补0
//  * @param {u8 *} dat
//  * @param {u8} len 最大不超过64
//  * @return {*}
//  */
// void encodeInfo(u8 * dat,u8 len,u8 * info)
// {
//     u8 l,i,j;

//     l=len/16;
//     if(len%16>0)
//         l++;
//     for(i=0;i<l;i++)
//     {
//         if(i==l-1 && len%16>0)
//         {
//             for(j=0;j<16-len%16;j++)
//             {
//                 buff[]
//             }
//         }else{
//             for(j=0;j<16;j++)
//             {
//                 buff[i*16+j]=dat[i*16+15-j];
//             }
//         }
//     }
//     HAL_LSCRYPT_AES_ECB_Encrypt(buff, len, rebuff);
//     for(i=0;i<l;i++)
//     {
//         for(j=0;j<16;j++)
//         {
//             info[i*16+j]=rebuff[i*16+15-j];
//         }
//     }
// }
/**
 * @description: 解析指令
 * 指令格式：时间戳（10位），加密码md5签名（16位小写），A：动作，P:参数
 * A:OPEN  开门
 * A:PW    修改密码  P:新密码的md5签名（16位小写）
 * 16376319554621d373cade4e83A:OPEN;
 * 16376319554621d373cade4e83A:PW;P:1859cb5ca6da0c48;
 * @param {u8 *} dat
 * @param {u8} len
 * @return {*}
 */
CMD_TYPE ParseCmd(const u8 *dat, u8 len, u8 *para)
{
    char *posBegin;
    char *posEnd;
    u8 tempPw[17] = {0};
    u8 curTime[11] = {0};
    u8 action[10] = {0};
    if (len < 20 || len % 16 > 0 || len > 64)
    {
        LOG_I("CMD_ERROR");
        return CMD_ERROR;
    }
    if (curState != SYS_IDEL)
    {
        LOG_I("CMD_BUSY");
        return CMD_BUSY;
    }

    LOG_I("decodeInfo");
    decodeInfo(dat, len);
    LOG_I("cmd:%s\0", buff);
    memcpy(curTime, buff, 10);
    //对比时间
    LOG_I("curTime:%s", curTime);
    LOG_I("lastTime:%s", lastTime);
    if (strcmp((const char *)curTime, (const char *)lastTime) <= 0 && strcmp((const char *)curTime,"0450720000")!=0)
    {
        LOG_I("CMD_OVERTIME");
        return CMD_OVERTIME;
    }
    memcpy(lastTime, curTime, 10);

    //获取action
    posBegin = strstr((const char *)buff, "A:");
    if (posBegin != NULL)
    {
        posEnd = strstr(posBegin, ";");
        if (posEnd != NULL)
        {
            memcpy(action, posBegin + 2, posEnd - posBegin - 2);
        }
        else
        {
            LOG_I("CMD_ERROR A");
            return CMD_ERROR;
        }
    }
    else
    {
        LOG_I("CMD_ERROR A");
        return CMD_ERROR;
    }
    //获取para
    posBegin = strstr((const char *)buff, "P:");
    if (posBegin != NULL)
    {
        posEnd = strstr((const char *)posBegin, ";");
        if (posEnd != NULL)
        {
            memcpy(para, posBegin + 2, posEnd - posBegin - 2);
        }
        else
        {
            LOG_I("CMD_ERROR P");
            return CMD_ERROR;
        }
    }
    else
    {
        LOG_I("CMD_ERROR P");
        return CMD_ERROR;
    }

    memcpy(tempPw, buff + 10, 16);
    LOG_I("tempPw:%s", tempPw);
    LOG_I("pw:%s", pw);
    //修改密码指令，如果新密码能对上，也算密码正确
    if (strcmp((const char *)tempPw, (const char *)pw) != 0 && strcmp((const char *)para, (const char *)pw) != 0)
    {
        LOG_I("CMD_PWERROR");
        return CMD_PWERROR;
    }

    //开门指令
    if (strcmp((const char *)action, "OPEN") == 0)
    {
        LOG_I("CMD_OPEN");
        return CMD_OPEN;
    }
    //修改密码指令
    else if (strcmp((const char *)action, "PW") == 0)
    {
        LOG_I("CMD_PW");
        return CMD_PW;
    }
    else
    {
        LOG_I("CMD_UNDEFINE");
        return CMD_UNDEFINE;
    }
}
