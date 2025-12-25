#include "stm32f10x.h"
#include "drv_adc1_dma.h"

void DRV_ADC1_Config(void)
{
    RCC->APB2ENR |= (1U << 2) | (1U << 9);
    RCC->CFGR &= ~(0x3U << 14);
    RCC->CFGR |=  (0x2U << 14);

    GPIOA->CRL &= ~((0xFU << 0) | (0xFU << 4) | (0xFU << 8));

    ADC1->SMPR2 |= (0x7U << 0) | (0x7U << 3) | (0x7U << 6);

    ADC1->CR2 |= (1U << 0);
}

void DRV_ADC1_DMA_Config(void)
{
    ADC1->CR1 |= (1U << 8);
    ADC1->CR2 |= (1U << 1) | (1U << 8);

    ADC1->CR2 &= ~(0x7U << 17);
    ADC1->CR2 |=  (0x7U << 17);
    ADC1->CR2 &= ~(1U << 20);
}

void DRV_DMA1_CH1_Config(uint16_t *buf, uint8_t len)
{
    RCC->AHBENR |= (1U << 0);
    DMA1_Channel1->CCR &= ~(1U << 0);

    DMA1_Channel1->CPAR = (uint32_t)&ADC1->DR;
    DMA1_Channel1->CMAR = (uint32_t)buf;
    DMA1_Channel1->CNDTR = len;

    DMA1_Channel1->CCR = 0;
    DMA1_Channel1->CCR |= (1U << 7);
    DMA1_Channel1->CCR |= (1U << 5);
    DMA1_Channel1->CCR |= (1U << 8);
    DMA1_Channel1->CCR |= (1U << 10);
    DMA1_Channel1->CCR |= (0x3U << 12);
    DMA1_Channel1->CCR &= ~(1U << 4);
    DMA1_Channel1->CCR |= (1U << 0);
}

void DRV_ADC1_StartConversion(void)
{
    ADC1->CR2 |= (1U << 0);
    ADC1->CR2 |= (1U << 22);
}
