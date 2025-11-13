#ifndef SYSTICK_H
#define SYSTICK_H
#include "stm32f10x.h"
#include <stdint.h>

void SysTick_Init(void);
uint32_t GetTick(void);
uint8_t Delay_Elapsed(uint32_t start, uint32_t ms);

#endif
