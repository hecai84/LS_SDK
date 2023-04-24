/*
 * @Description:
 * @Author: hecai
 * @Date: 2021-09-14 15:27:57
 * @LastEditTime: 2023-04-11 10:35:09
 * @FilePath: \ble\ble_lock\Motor.c
 */
#include "Motor.h"
#include "Tools.h"
#include "io_config.h"
#include "platform.h"
#include "lsiwdg.h"
#include "cpu.h"

#define IN1 PB09
#define IN2 PB08
#define CHECK_TIME 50
#define OVER_CURRENT

static struct builtin_timer *motor_timer_inst_stop = NULL;
static struct builtin_timer *motor_timer_inst_close = NULL;
static struct builtin_timer *motor_timer_inst_finish = NULL;
static struct builtin_timer *iwdg_timer_inst = NULL;
static bool foward = true;
static bool isWorking = false;
static u16 openTime = 0;
static u16 pauseTime = 0;
static u16 closeTime = 0;

void iwdgTimer(void *param)
{
    if(isWorking)
    {
        HAL_IWDG_Refresh();
        builtin_timer_start(iwdg_timer_inst, 500, NULL);
    }else
    {
        HAL_IWDG_DeInit();
        builtin_timer_stop(iwdg_timer_inst);
    }
}

void initMotor(void)
{
    io_cfg_output(IN1);
    io_write_pin(IN1, 0);
    io_cfg_output(IN2);
    io_write_pin(IN2, 0);
}

void stopMotor(void *param)
{
    io_write_pin(IN1, 0);
    io_write_pin(IN2, 0);
}

void closeDoor(void *param)
{
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
    return;
}

void powFinish(void *param)
{
    stopMotor(NULL);
    isWorking = false;
    UpdateBattery();
}

// void startMotor(void *param)
// {
//     uint32_t cpu_stat = enter_critical();
//     LOG_I("openDoor");
//     // 正转
//     io_write_pin(IN1, 0);
//     io_write_pin(IN2, 0);
//     if (foward)
//         io_write_pin(IN1, 1);
//     else
//         io_write_pin(IN2, 1);
//     DELAY_US(openTime * 1000);
//     // 停机
//     io_write_pin(IN1, 0);
//     io_write_pin(IN2, 0);
//     DELAY_US(pauseTime * 1000);
//     // 反转
//     if (foward)
//         io_write_pin(IN2, 1);
//     else
//         io_write_pin(IN1, 1);
//     DELAY_US(closeTime * 1000);
//     // 停机
//     io_write_pin(IN1, 0);
//     io_write_pin(IN2, 0);
//     exit_critical(cpu_stat);
// }

// void openDoor(int procTime, int waitTime, int reTime)
// {
//     LOG_I("procTime:%d waitTime:%d", procTime, waitTime, reTime);
//     if (procTime > 0)
//     {
//         foward = true;
//         openTime = procTime;
//     }
//     else
//     {
//         foward = false;
//         openTime = -procTime;
//     }
//     pauseTime = waitTime;
//     closeTime = reTime;

//     if (motor_timer_inst == NULL)
//         motor_timer_inst = builtin_timer_create(startMotor);
    
    
//     builtin_timer_stop(motor_timer_inst);
//     builtin_timer_start(motor_timer_inst, 500, NULL);
// }

void openDoor(int procTime,int waitTime,int reTime)
{
    LOG_I("procTime:%d waitTime:%d",procTime,waitTime,reTime);

    if (procTime > 0)
    {
        foward = true;
        openTime = procTime;
    }
    else
    {
        foward = false;
        openTime = -procTime;
    }
    pauseTime = waitTime;
    closeTime = reTime;
    if(isWorking)
        return;
    isWorking = true;
    // 开启看门狗
    HAL_IWDG_Init(32768);
    if (iwdg_timer_inst == NULL)
        iwdg_timer_inst = builtin_timer_create(iwdgTimer);
    else
        builtin_timer_stop(iwdg_timer_inst);
    builtin_timer_start(iwdg_timer_inst, 500, NULL);

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
    builtin_timer_start(motor_timer_inst_stop, openTime, NULL);
    //定时反转
    builtin_timer_stop(motor_timer_inst_close);
    builtin_timer_start(motor_timer_inst_close, openTime+pauseTime, NULL);
    //定时结束
    builtin_timer_stop(motor_timer_inst_finish);
    builtin_timer_start(motor_timer_inst_finish, openTime+pauseTime+closeTime, NULL);

    //    adc_timer_inst=builtin_timer_create(checkCurrent);
    //    recv_flag=0;
    //    HAL_ADC_Start_IT(&hadc);
    //    builtin_timer_start(adc_timer_inst, CHECK_TIME, NULL);
    
}
