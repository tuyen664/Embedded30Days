#ifndef BSP_TIME_H
#define BSP_TIME_H

#include <stdint.h>

void BSP_Time_Init(uint32_t sysclk_hz);
void BSP_DelayMs(uint32_t ms);
uint32_t BSP_Millis(void);

#endif
