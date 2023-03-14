/*
 * @Description: 
 * @Author: hecai
 * @Date: 2021-11-02 23:24:40
 * @LastEditTime: 2023-03-11 00:08:06
 * @FilePath: \ble\ble_lock_adapter\flash.c
 */
#include "flash.h"
#include "log.h"
#include "string.h"

extern SLAVE_DATA slave_array[];
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


void clearSlaveArray()
{
    memset(slave_array, 0, sizeof(SLAVE_DATA) * MAX_SLAVE);
}


void loadSlave(SLAVE_DATA *data,uint8_t * len)
{
    uint16_t srcLen;
    uint8_t i;
    uint8_t buff[120];
    readFlash(REOCDE_SLAVE, buff, &srcLen);
    LOG_I("readFlash:%d", srcLen);
    if(srcLen%21==0)
    {
        *len = srcLen / 21;
        for (i = 0; i < *len;i++)
        {
            data[i].mac[0] = buff[21 * i + 5];
            data[i].mac[1] = buff[21 * i + 4];
            data[i].mac[2] = buff[21 * i + 3];
            data[i].mac[3] = buff[21 * i + 2];
            data[i].mac[4] = buff[21 * i + 1];
            data[i].mac[5] = buff[21 * i + 0];
            LOG_I("mac:%02X-%02X-%02X-%02X-%02X-%02X", 
                data[i].mac[0],data[i].mac[1],data[i].mac[2],data[i].mac[3],data[i].mac[4],data[i].mac[5]);
            memcpy(data[i].pw, buff+21*i+BLE_ADDR_LEN, 8);
            data[i].flag=buff[21*i+14];
            data[i].openTime=buff[21*i+15]*256+buff[21*i+16];
            LOG_I("openTime%d", data[i].openTime);
            data[i].waitTime=buff[21*i+17]*256+buff[21*i+18];
            LOG_I("waitTime%d", data[i].waitTime);
            data[i].closeTime=buff[21*i+19]*256+buff[21*i+20];
            LOG_I("closeTime%d", data[i].closeTime);
        }
    }else
        *len = 0;
}
uint8_t saveSlave(const uint8_t *data,uint8_t len)
{
    uint8_t buff[120];
    LOG_I("writeFlash");
    memcpy(buff, data, len);
    return writeFlash(REOCDE_SLAVE, buff, len);
}

