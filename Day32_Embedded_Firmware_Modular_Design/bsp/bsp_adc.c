#include "bsp_adc.h"
#include "drv_adc1_dma.h"

void BSP_ADC_Init(uint16_t *buf, uint8_t channels)
{
    DRV_ADC1_Config();
    DRV_ADC1_DMA_Config();
    DRV_DMA1_CH1_Config(buf, channels);
}

void BSP_ADC_Start(void)
{
    DRV_ADC1_StartConversion();
}
