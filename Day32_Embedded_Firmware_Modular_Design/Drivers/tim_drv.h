#ifndef TIM_DRV_H
#define TIM_DRV_H

#include <stdint.h>

void DRV_TIM_PWM_Init(uint32_t tim);
void DRV_TIM_PWM_SetPulse(uint32_t tim, uint32_t channel, uint16_t pulse);

#endif
