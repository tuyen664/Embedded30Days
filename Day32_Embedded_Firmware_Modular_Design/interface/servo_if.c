#include "servo_if.h"
#include "bsp_servo.h"

void Servo_Init(void)
{
    BSP_Servo_Init();
}

void Servo_SetAngle(uint8_t angle)
{
    BSP_Servo_SetAngle(angle);
}
