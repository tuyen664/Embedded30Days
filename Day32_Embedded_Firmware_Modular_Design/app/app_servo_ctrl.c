#include "app_servo_ctrl.h"

#include <stdio.h>
#include <stdlib.h>

#include "uart_if.h"
#include "servo_if.h"
#include "time_if.h"

#define VREF        3.3f
#define ADC_RES     4095.0f
#define MANUAL_TIMEOUT_MS 5000

static uint16_t *s_adc_buf;
static uint8_t  s_adc_channels;

static uint8_t  manual_mode = 0;
static uint32_t last_manual_time = 0;

static char rx_buf[16];
static uint8_t rx_idx = 0;

static uint8_t servo_angle = 90;
static uint32_t last_auto_update = 0;

void App_ServoCtrl_Init(uint16_t *adc_buf, uint8_t channels)
{
    s_adc_buf = adc_buf;
    s_adc_channels = channels;

    servo_angle = 90;
    Servo_SetAngle(servo_angle);
}


void App_ServoCtrl_Task(void)
{
    char msg[64];
    float volts[3];

    /* -------- UART MANUAL MODE -------- */
    if (Console_CharAvailable())
    {
        char c = Console_ReadChar();
			
			  char echo[2] = {c, '\0'};
        Console_Print(echo);

        if (rx_idx == 0)
        {
            manual_mode = 1;
            last_manual_time = Time_Millis();
        }

        if (c == '\r' || c == '\n')
        {
            rx_buf[rx_idx] = '\0';

            int angle = atoi(rx_buf);
            if (angle < 0) angle = 0;
            if (angle > 180) angle = 180;

            Servo_SetAngle((uint8_t)angle);

            sprintf(msg, "\r\nSet servo %d deg\r\n", angle);
            Console_Print(msg);

            rx_idx = 0;
            manual_mode = 1;
            last_manual_time = Time_Millis();
        }
        else if (c >= '0' && c <= '9' && rx_idx < sizeof(rx_buf) - 1)
        {
            rx_buf[rx_idx++] = c;
					  

        }
        else
        {
            /* ignore invalid char */
        }
    }

    /* -------- AUTO MODE -------- */
    if (manual_mode == 0 ||
       (Time_Millis() - last_manual_time > MANUAL_TIMEOUT_MS))
    {
        manual_mode = 0;

        if (Time_Millis() - last_auto_update > 500)
        {
            last_auto_update = Time_Millis();

            for (uint8_t ch = 0; ch < s_adc_channels; ch++)
            {
                volts[ch] = (s_adc_buf[ch] * VREF) / ADC_RES;
            }

            sprintf(msg,
                    "CH1=%.2fV  CH2=%.2fV  CH3=%.2fV\r\n",
                    volts[0], volts[1], volts[2]);
            Console_Print(msg);

            float left  = volts[0];
            float right = volts[2];

            if (left > right + 0.3f)
                servo_angle = 180;
            else if (right > left + 0.3f)
                servo_angle = 0;
            else
                servo_angle = 90;

            Servo_SetAngle(servo_angle);
        }
    }
}
