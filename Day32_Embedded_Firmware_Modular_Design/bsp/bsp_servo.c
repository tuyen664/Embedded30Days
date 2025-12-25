#include "bsp_servo.h"
#include "drv_tim4_pwm.h"


#define SERVO_MIN_US    600U
#define SERVO_MAX_US    2400U

void BSP_Servo_Init(void)
{
    DRV_TIM4_PWM_Init();
}

void BSP_Servo_SetAngle(uint8_t angle)
{
    if (angle > 180) angle = 180;

    uint32_t pulse = SERVO_MIN_US +
                     ((SERVO_MAX_US - SERVO_MIN_US) * angle) / 180U;

    
    DRV_TIM4_SetCCR(pulse);
}
