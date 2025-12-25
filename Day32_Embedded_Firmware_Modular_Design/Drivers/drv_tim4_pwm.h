#ifndef DRV_TIM4_PWM_H
#define DRV_TIM4_PWM_H

#include <stdint.h>

void DRV_TIM4_PWM_Init(void);
void DRV_TIM4_SetCCR(uint32_t value);

#endif
