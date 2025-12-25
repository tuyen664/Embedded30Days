#ifndef TIME_IF_H
#define TIME_IF_H

#include <stdint.h>

void Time_Init(uint32_t sysclk_hz);
void Time_DelayMs(uint32_t ms);
uint32_t Time_Millis(void);

#endif
