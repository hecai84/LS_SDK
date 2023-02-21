/*
 * @Description: 
 * @Author: hecai
 * @Date: 2021-11-02 23:24:46
 * @LastEditTime: 2023-02-19 15:37:50
 * @FilePath: \ble\ble_lock_adapter\flash.h
 */
#ifndef __FLASH_H__
#define __FLASH_H__

#include <stddef.h>
#include "ls_ble.h"

#define MAX_SLAVE 10
typedef enum{
    RECODE_PW=1,
    REOCDE_SLAVE,
    RECODE_OTHER
}RECODE_NAME;

typedef struct _SLAVE_DATA
{
    uint8_t mac[BLE_ADDR_LEN];
    uint8_t pw[16];
    uint8_t flag;//0xff处理完，0空设备,1 正传，2反转
    uint16_t openTime;
    uint16_t waitTime;
    uint16_t closeTime;
} SLAVE_DATA;

void initFlash(void);
uint8_t writeFlash(uint16_t recode_name, uint8_t *data, uint16_t len);
void readFlash(uint16_t recode_name, uint8_t *data, uint16_t *len);
void loadSlave(SLAVE_DATA *data, uint8_t *len);

#endif

