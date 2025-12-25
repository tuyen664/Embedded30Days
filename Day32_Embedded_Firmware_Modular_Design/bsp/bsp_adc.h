#ifndef BSP_ADC_H
#define BSP_ADC_H

#include <stdint.h>

void BSP_ADC_Init(uint16_t *buf, uint8_t channels);
void BSP_ADC_Start(void);

#endif
