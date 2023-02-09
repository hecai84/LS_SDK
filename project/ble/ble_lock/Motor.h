/*
 * @Description: 
 * @Author: hecai
 * @Date: 2021-09-14 15:28:12
 * @LastEditTime: 2022-08-14 22:04:10
 * @FilePath: \ls_ble_sdk\dev\project\ble\ble_uart_server\Motor.h
 */
#ifndef __MOTOR_H__
#define __MOTOR_H__
void initMotor(void);
void openDoor(int procTime,int waitTime,int reTime);
void closeDoor(void *param);
void stopMotor(void *param);



#endif

