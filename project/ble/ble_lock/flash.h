/*
 * @Description: 
 * @Author: hecai
 * @Date: 2021-11-02 23:24:46
 * @LastEditTime: 2021-11-02 23:58:36
 * @FilePath: \ls_ble_sdk\dev\project\ble\ble_uart_server\flash.h
 */
#ifndef __FLASH_H__
#define __FLASH_H__

typedef enum{
    RECODE_PW=1,
    RECODE_OTHER
}RECODE_NAME;

void initFlash(void);
uint8_t writeFlash(uint16_t recode_name, uint8_t *data, uint16_t len);
void readFlash(uint16_t recode_name, uint8_t *data, uint16_t *len);



#endif

