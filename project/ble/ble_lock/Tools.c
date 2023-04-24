/*
 * @Description:
 * @Author: hecai
 * @Date: 2021-09-16 10:44:14
 * @LastEditTime: 2023-04-12 00:29:40
 * @FilePath: \ble\ble_lock\Tools.c
 */
#include "Tools.h"
#include "lscrypt.h"
#include "lsadc.h"
#include <string.h>
#include "flash.h"
#include "platform.h"
/**buff**/
uint8_t ciphertext_buff[64];
uint8_t plaintext_buff[64];
extern u8 pw[];
extern u8 lastTime[];

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

static struct builtin_timer *led_timer_blue = NULL;
static struct builtin_timer *led_timer_red = NULL;
static struct builtin_timer *click_timer = NULL;
static struct builtin_timer *chrg_timer = NULL;
static u8 twinkRedTimes;
static u8 twinkBlueTimes;
volatile uint8_t recv_flag = 1;
static uint16_t adc_value;
ADC_HandleTypeDef hadc;
u8 battery = 5;
u8 redState = 1;
#include "main.h"

void Error_Handler(void);

void initLed(void)
{
    io_cfg_output(LED_RED); // LEDR
    // io_cfg_opendrain(LED_RED);
    io_write_pin(LED_RED, 1);
    io_cfg_output(LED_BLUE); // LEDB
    // io_cfg_opendrain(LED_BLUE);
    io_write_pin(LED_BLUE, 1);
}

/*
在LE501X系统中，只有PA00、PA07、PB11、PB15四个IO可以在任
意状态下触发中断，其余IO只能在CPU工作状态下触发中断，不能在BLE休眠状态下触发中断。
BLE处于广播或者连接时，系统根据广播、连接的间隔定期自动休眠、唤醒，所以这种应用场景，
建议使用上述四根IO作为中断。
*/
void initExti(void)
{
    io_cfg_input(BT);                     // PB11 config input
    io_pull_write(BT, IO_PULL_UP);        // PB11 config pullup
    io_exti_config(BT, INT_EDGE_FALLING); // PB11 interrupt falling edge
    io_exti_enable(BT, true);             // PB11 interrupt enable

    io_cfg_input(CHRG);                     // PB11 config input
    io_pull_write(CHRG, IO_PULL_UP);        // PB11 config pullup
    io_exti_config(CHRG, INT_EDGE_FALLING); // PB11 interrupt falling edge
    io_exti_enable(CHRG, true);             // PB11 interrupt enable
}

void toggleRed(void *param)
{
    if (twinkRedTimes != 255)
        twinkRedTimes--;
    redState = !redState;
    io_write_pin(LED_RED, redState);
    if (led_timer_red && twinkRedTimes > 0)
    {
        if (redState == 0)
        {
            builtin_timer_start(led_timer_red, TWINK_RED_TIME1, NULL);
        }
        else
        {
            builtin_timer_start(led_timer_red, TWINK_RED_TIME2, NULL);
        }
    }
}
void toggleBlue(void *param)
{
    twinkBlueTimes--;
    io_toggle_pin(LED_BLUE);
    if (led_timer_blue && twinkBlueTimes > 0)
    {
        builtin_timer_start(led_timer_blue, TWINK_BLUE_TIME, NULL);
    }
    else
    {
        io_write_pin(LED_BLUE, 1);
    }
}
void longPress(void *param)
{
    if (io_read_pin(BT) == 0)
    {
        LOG_I("longPress");
        SetLedBlue(LED_TWINK);
        memcpy(pw, DEFAULT_PW, 16);
        LOG_I("new pw:%s", pw);
        memset(lastTime, 0, 11);
        writeFlash(RECODE_PW, pw, 16);
    }
}
void chrgcheck(void *param)
{
    if (io_read_pin(CHRG) == 0)
    {
        SetLedRed(LED_OPEN);
        if (chrg_timer)
            builtin_timer_start(chrg_timer, CHRG_OVERTIME, NULL);
    }
    else
    {
        LOG_I("charge out");
        UpdateBattery();
        updateAdv();
        SetLedRed(LED_CLOSE);
    }
}

/** Common config  */
static void lsadc_init(void)
{
}

void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    adc_value = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);
    recv_flag = 1;
}


void SetLedRed(LED_STATE state)
{
    twinkRedTimes = 255;
    if (state == LED_TWINK)
    {
        LOG_I("SetLedRed TWINK");
        if (led_timer_red == NULL)
        {
            led_timer_red = builtin_timer_create(toggleRed);
        }
        io_write_pin(LED_RED, 0);
        redState = 0;
        LOG_I("star SetLedRed TWINK");
        builtin_timer_start(led_timer_red, 500, NULL);
    }
    else
    {
        if (led_timer_red != NULL)
        {
            builtin_timer_stop(led_timer_red);
        }
        if (state == LED_OPEN)
            io_write_pin(LED_RED, 0);
        if (state == LED_CLOSE)
            io_write_pin(LED_RED, 1);
    }
}

void SetLedBlue(LED_STATE state)
{
    if (state == LED_TWINK)
    {
        twinkBlueTimes = 10;
        LOG_I("SetLedBlue TWINK");
        if (led_timer_blue == NULL)
            led_timer_blue = builtin_timer_create(toggleBlue);
        io_write_pin(LED_BLUE, 0);
        builtin_timer_start(led_timer_blue, TWINK_BLUE_TIME, NULL);
    }
    else
    {
        if (state == LED_OPEN)
        {
            LOG_I("SetLedBlue OPEN");
            io_write_pin(LED_BLUE, 0);
        }
        if (state == LED_CLOSE)
        {
            LOG_I("SetLedBlue CLOSE");
            io_write_pin(LED_BLUE, 1);
        }
    }
}

void io_exti_callback(uint8_t pin)
{
    LOG_I("exti");
    switch (pin)
    {
    case BT:
        // do something

        LOG_I("click");
        if (click_timer == NULL)
        {
            // builtin_timer_stop(click_timer);
            click_timer = builtin_timer_create(longPress);
        }
        builtin_timer_stop(click_timer);
        builtin_timer_start(click_timer, LONGPRESS_TIME, NULL);
        break;
    case CHRG:
        LOG_I("charging");
        battery = 6;
        updateAdv();
        SetLedRed(LED_OPEN);
        if (chrg_timer == NULL)
        {
            chrg_timer = builtin_timer_create(chrgcheck);
        }
        builtin_timer_stop(chrg_timer);
        builtin_timer_start(chrg_timer, CHRG_OVERTIME, NULL);
        break;
    default:
        break;
    }
}



void UpdateBattery(void)
{
    uint16_t value;
    hadc.Instance = LSADC;
    hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc.Init.ScanConvMode = ADC_SCAN_DISABLE;        /* Sequencer disabled (ADC conversion on only 1 channel: channel set on rank 1) */
    hadc.Init.NbrOfConversion = 1;                    /* Parameter discarded because sequencer is disabled */
    hadc.Init.DiscontinuousConvMode = DISABLE;        /* Parameter discarded because sequencer is disabled */
    hadc.Init.NbrOfDiscConversion = 1;                /* Parameter discarded because sequencer is disabled */
    hadc.Init.ContinuousConvMode = DISABLE;           /* Continuous mode to have maximum conversion speed (no delay between conversions) */
    hadc.Init.TrigType = ADC_INJECTED_SOFTWARE_TRIGT; /* The reference voltage uses an internal reference */
    hadc.Init.Vref = ADC_VREF_INSIDE;
    hadc.Init.AdcCkDiv = ADC_CLOCK_DIV32;
    if (HAL_ADC_Init(&hadc) != HAL_OK)
    {
        Error_Handler();
    }
    adc12b_in6_io_init();
    ADC_InjectionConfTypeDef sConfigInjected = {0};
    sConfigInjected.InjectedChannel = ADC_CHANNEL_6;
    sConfigInjected.InjectedRank = ADC_INJECTED_RANK_1;
    sConfigInjected.InjectedSamplingTime = ADC_SAMPLETIME_15CYCLES;
    sConfigInjected.InjectedOffset = 0;
    sConfigInjected.InjectedNbrOfConversion = 1;
    sConfigInjected.InjectedDiscontinuousConvMode = DISABLE;
    sConfigInjected.AutoInjectedConv = DISABLE;

    if (HAL_ADCEx_InjectedConfigChannel(&hadc, &sConfigInjected) != HAL_OK)
    {
        /* Channel Configuration Error */
        Error_Handler();
    }

    recv_flag = 0;
    HAL_ADCEx_InjectedStart_IT(&hadc);
    LOG_I("start adc");
    while (recv_flag == 0)
        DELAY_US(100);

    value = 6 * 1400 * adc_value / 4095;
    LOG_I("battery:%d", value);
    if (value < 3200)
    {
        LOG_I("twink red");
        SetLedRed(LED_TWINK);
    }
    else if (io_read_pin(CHRG) != 0)
    {
        SetLedRed(LED_CLOSE);
    }
    if (value > 3950)
        battery = 5;
    else if (value > 3800)
        battery = 4;
    else if (value > 3600)
        battery = 3;
    else if (value > 3400)
        battery = 2;
    else
        battery = 1;

    HAL_ADC_DeInit(&hadc);
}
/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */

    /* USER CODE END Error_Handler_Debug */
        LOG_I("Error_Handler");
}
