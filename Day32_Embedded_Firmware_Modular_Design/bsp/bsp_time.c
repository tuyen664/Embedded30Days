#include "bsp_time.h"
#include "drv_systick.h"

void BSP_Time_Init(uint32_t sysclk_hz)
{
    DRV_SysTick_Init(sysclk_hz);
}

void BSP_DelayMs(uint32_t ms)
{
    DRV_DelayMs(ms);
}

uint32_t BSP_Millis(void)
{
    return DRV_Millis();
}
