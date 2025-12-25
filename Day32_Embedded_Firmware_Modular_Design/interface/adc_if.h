#ifndef ADC_IF_H
#define ADC_IF_H

#include <stdint.h>

void SensorADC_Init(uint16_t *buf, uint8_t channels);
void SensorADC_Start(void);

#endif
