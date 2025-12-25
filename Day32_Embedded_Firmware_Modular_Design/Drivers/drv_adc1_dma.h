#ifndef DRV_ADC1_DMA_H
#define DRV_ADC1_DMA_H

#include <stdint.h>

void DRV_ADC1_Config(void);
void DRV_ADC1_DMA_Config(void);
void DRV_DMA1_CH1_Config(uint16_t *buf, uint8_t len);
void DRV_ADC1_StartConversion(void);

#endif
