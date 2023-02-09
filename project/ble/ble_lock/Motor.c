/*
 * @Description:
 * @Author: hecai
 * @Date: 2021-09-14 15:27:57
 * @LastEditTime: 2022-08-14 22:03:46
 * @FilePath: \ls_ble_sdk\dev\project\ble\ble_uart_server\Motor.c
 */
#include "Motor.h"
#include "Tools.h"
#include "io_config.h"

#define IN1 PB09
#define IN2 PB08
#define CHECK_TIME 50
#define OVER_CURRENT

static struct builtin_timer *motor_timer_inst_stop = NULL;
static struct builtin_timer *motor_timer_inst_close = NULL;
static struct builtin_timer *motor_timer_inst_finish = NULL;
static bool foward = true;
bool isWorking = false;
u16 processTime = 0;
u16 returnTime = 0;

void initMotor(void)
{
    io_cfg_output(IN1);
    io_write_pin(IN1, 0);
    io_cfg_output(IN2);
    io_write_pin(IN2, 0);
}

void stopMotor(void *param)
{
    LOG_I("stopMotor");
    io_write_pin(IN1, 0);
    io_write_pin(IN2, 0);
}

void closeDoor(void *param)
{
    LOG_I("closeDoor");
    io_write_pin(IN1, 0);
    io_write_pin(IN2, 0);
    if (foward)
    {
        io_write_pin(IN2, 1);
    }
    else
    {
        io_write_pin(IN1, 1);
    }
    
    //定时关闭
    builtin_timer_stop(motor_timer_inst_stop);
    builtin_timer_start(motor_timer_inst_stop, returnTime, NULL);
    return;
}

void powFinish(void *param)
{
    isWorking = false;
    stopMotor(NULL);
    UpdateBattery();
}

void openDoor(int procTime,int waitTime,int reTime)
{
    LOG_I("procTime:%d waitTime:%d",procTime,waitTime,reTime);
    if (procTime > 0)
    {
        foward = true;
        processTime = procTime;
    }
    else
    {
        foward = false;
        processTime = -procTime;
    }
    returnTime = reTime;
    if(isWorking)
        return;
    isWorking = true;
    if (motor_timer_inst_stop == NULL)
        motor_timer_inst_stop = builtin_timer_create(stopMotor);
    if (motor_timer_inst_close == NULL)
        motor_timer_inst_close = builtin_timer_create(closeDoor);
    if (motor_timer_inst_finish == NULL)
        motor_timer_inst_finish = builtin_timer_create(powFinish);

    LOG_I("openDoor");
    io_write_pin(IN1, 0);
    io_write_pin(IN2, 0);
    if (foward)
    {
        io_write_pin(IN1, 1);
    }
    else
    {
        io_write_pin(IN2, 1);
    }
    //定时关闭
    builtin_timer_stop(motor_timer_inst_stop);
    builtin_timer_start(motor_timer_inst_stop, processTime, NULL);
    //定时反转
    builtin_timer_stop(motor_timer_inst_close);
    builtin_timer_start(motor_timer_inst_close, waitTime+processTime, NULL);
    //定时结束
    builtin_timer_stop(motor_timer_inst_finish);
    builtin_timer_start(motor_timer_inst_finish, waitTime+processTime*3, NULL);

    //    adc_timer_inst=builtin_timer_create(checkCurrent);
    //    recv_flag=0;
    //    HAL_ADC_Start_IT(&hadc);
    //    builtin_timer_start(adc_timer_inst, CHECK_TIME, NULL);

    return;
}
