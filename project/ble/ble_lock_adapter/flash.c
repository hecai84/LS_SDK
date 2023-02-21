/*
 * @Description: 
 * @Author: hecai
 * @Date: 2021-11-02 23:24:40
 * @LastEditTime: 2023-02-20 09:57:29
 * @FilePath: \ble\ble_lock_adapter\flash.c
 */
#include "tinyfs.h"
#include "flash.h"
#include "log.h"
#include "string.h"

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
    uint8_t re;
    re = tinyfs_read(ble_at_dir, recode_name, data,len);
    if (re != TINYFS_NO_ERROR)
    {
        LOG_I("readFlash error:%d", re);
    }
}

void loadSlave(SLAVE_DATA *data, uint8_t *len)
{
    //测试数据
    data[0].mac[0] = 0xae;
    data[0].mac[1] = 0x2a;
    data[0].mac[2] = 0xb8;
    data[0].mac[3] = 0x43;
    data[0].mac[4] = 0x69;
    data[0].mac[5] = 0xef;
    memcpy(data[0].pw, "49ba59abbe56e057", 16);
    data[0].flag = 1;
    data[0].openTime = 600;
    data[0].waitTime = 2000;
    data[0].closeTime = 600;
    *len = 1;

    // uint16_t srcLen;
    // uint8_t size = sizeof(SLAVE_DATA);
    // readFlash(REOCDE_SLAVE, (uint8_t *)data, &srcLen);
    // if(srcLen>0 && srcLen%size==0)
    // {
    //     *len = srcLen / size;
    // }else
    // {
    //     *len = 0;
    // }
}
void saveSlave(SLAVE_DATA *data,uint8_t len)
{
    uint8_t size = sizeof(SLAVE_DATA);
    writeFlash(REOCDE_SLAVE, (uint8_t *)data, len * size);
}

