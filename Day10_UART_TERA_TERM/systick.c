#include "systick.h"

volatile uint32_t sysTickMs = 0;

void SysTick_Init(void)
{
    SysTick->LOAD = (SystemCoreClock / 1000) - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = 7; // ENABLE | TICKINT | CLKSOURCE
}

void SysTick_Handler(void)
{
    sysTickMs++;
}

uint32_t GetTick(void)
{
    return sysTickMs;
}

uint8_t Delay_Elapsed(uint32_t start, uint32_t ms)
{
    return (GetTick() - start) >= ms; // return 1 or 0
}
