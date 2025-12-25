#include "app_main.h"

#include "time_if.h"
#include "uart_if.h"
#include "adc_if.h"
#include "servo_if.h"

#include "app_servo_ctrl.h"

#define SYSCLK_HZ 72000000UL
#define ADC_CHANNELS 3

static uint16_t adc_buf[ADC_CHANNELS];

void App_Init(void)
{
    Time_Init(SYSCLK_HZ);
    Console_Init();
    Servo_Init();

    SensorADC_Init(adc_buf, ADC_CHANNELS);
    SensorADC_Start();

    Console_Print("\r\n=== Mini Robot Ready ===\r\n");

    App_ServoCtrl_Init(adc_buf, ADC_CHANNELS);
}

void App_Run(void)
{
    while (1)
    {
        App_ServoCtrl_Task();
        Time_DelayMs(10);
    }
}
