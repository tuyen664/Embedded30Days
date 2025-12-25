#ifndef APP_SERVO_CTRL_H
#define APP_SERVO_CTRL_H

#include <stdint.h>

void App_ServoCtrl_Init(uint16_t *adc_buf, uint8_t channels);
void App_ServoCtrl_Task(void);

#endif
