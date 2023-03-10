/*
 * @Description: 
 * @Author: hecai
 * @Date: 2021-09-15 12:26:42
 * @LastEditTime: 2023-02-27 23:54:01
 * @FilePath: \ble\ble_lock_adapter\Tools.h
 */
#ifndef __TOOLS_H__
#define __TOOLS_H__
#include "builtin_timer.h"
#include "io_config.h"
#include <stddef.h>
#define LOG_TAG "BLE"
#include "log.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
//默认密码 123456  md5 16位小写
#define DEFAULT_PW "49ba59abbe56e057"

#define BT PB15
#define CHRG PA00
#define LED_RED PA14
#define LED_BLUE PA13
#define TWINK_BLUE_TIME 500
#define TWINK_RED_TIME1 500
#define TWINK_RED_TIME2 1500
#define LONGPRESS_TIME 3000
#define CHRG_OVERTIME 500
#define REFRESH_VBAT_TIME 60000

typedef enum 
{
    LED_OPEN = 0,
    LED_CLOSE,
    LED_TWINK,
}LED_STATE;

void initLed(void);
void initExti(void);
void SetLedRed(LED_STATE state);
void SetLedBlue(LED_STATE state);
void UpdateBattery(void);
void lsadc_init(void);


#endif
