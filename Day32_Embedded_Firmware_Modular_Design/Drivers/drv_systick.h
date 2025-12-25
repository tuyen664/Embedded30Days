#ifndef DRV_SYSTICK_H
#define DRV_SYSTICK_H

#include <stdint.h>

void DRV_SysTick_Init(uint32_t sysclk_hz);
uint32_t DRV_Millis(void);
void DRV_DelayMs(uint32_t ms);

#endif
