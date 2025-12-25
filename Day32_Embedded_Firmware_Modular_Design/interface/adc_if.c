#include "adc_if.h"
#include "bsp_adc.h"

void SensorADC_Init(uint16_t *buf, uint8_t channels)
{
    BSP_ADC_Init(buf, channels);
}

void SensorADC_Start(void)
{
    BSP_ADC_Start();
}
