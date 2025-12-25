#include "stm32f10x.h"
#include "drv_systick.h"

static volatile uint32_t tick_ms = 0;

void DRV_SysTick_Init(uint32_t sysclk_hz)
{
    SysTick->LOAD = (sysclk_hz / 1000U) - 1U;
    SysTick->VAL  = 0;
    SysTick->CTRL = 7U;
}

void SysTick_Handler(void)
{
    tick_ms++;
}

uint32_t DRV_Millis(void)
{
    return tick_ms;
}

void DRV_DelayMs(uint32_t ms)
{
    uint32_t start = tick_ms;
    while ((tick_ms - start) < ms);
}
