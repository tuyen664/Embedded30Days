#include "time_if.h"
#include "bsp_time.h"

void Time_Init(uint32_t sysclk_hz)
{
    BSP_Time_Init(sysclk_hz);
}

void Time_DelayMs(uint32_t ms)
{
    BSP_DelayMs(ms);
}

uint32_t Time_Millis(void)
{
    return BSP_Millis();
}
