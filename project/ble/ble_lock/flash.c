/*
 * @Description: 
 * @Author: hecai
 * @Date: 2021-11-02 23:24:40
 * @LastEditTime: 2021-12-15 01:39:16
 * @FilePath: \ls_ble_sdk\dev\project\ble\ble_uart_server\flash.c
 */
#include "tinyfs.h"
#include "log.h"
tinyfs_dir_t ble_at_dir;

void initFlash()
{
    tinyfs_mkdir(&ble_at_dir, ROOT_DIR, 5);
}

uint8_t writeFlash(uint16_t recode_name, uint8_t *data, uint16_t len)
{
    uint8_t re;
    re = tinyfs_write(ble_at_dir, recode_name, data, len);
    if(re != TINYFS_NO_ERROR)
    {
        LOG_I("writeFlash error:%d",re);
    }
    
    tinyfs_write_through();
    return re;
}
void readFlash(uint16_t recode_name, uint8_t *data, uint16_t *len)
{
    int re;
    re = tinyfs_read(ble_at_dir, recode_name, data,len);
    if (re != TINYFS_NO_ERROR)
    {
        LOG_I("readFlash error:%d", re);
    }
}

