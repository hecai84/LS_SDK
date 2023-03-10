/*
 * @Description: 
 * @Author: hecai
 * @Date: 2021-11-02 10:00:55
 * @LastEditTime: 2023-03-10 17:12:24
 * @FilePath: \ble\ble_lock_adapter\parser.h
 */
#ifndef __PARSER_H__
#define __PARSER_H__

#include <stdio.h>
#include "Tools.h"
#include "flash.h"

typedef enum{
    CMD_ERROR =0,
    CMD_BUSY,
    CMD_OPEN,
    CMD_PW,
    CMD_PWERROR,
    CMD_OVERTIME,
    CMD_UNDEFINE
}CMD_TYPE;
typedef enum{
    SYS_IDEL =0,
    SYS_WORKING,
}SYS_STATE;
    
CMD_TYPE ParseCmd(const u8 * dat,u8 len,u8 * para);
void encodeInfo(const u8 *dat, u8 len);
void encodeSlave(SLAVE_DATA *slave, u8 **dat, u8 *len);
void crypt_init(void);
void crypt_deinit(void);
void SetSysState(SYS_STATE state);

#endif

