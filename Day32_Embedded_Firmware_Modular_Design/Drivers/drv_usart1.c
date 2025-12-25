#include "stm32f10x.h"
#include "drv_usart1.h"

#define BAUD_9600_BRR   0x1D4C

void DRV_USART1_Init(void)
{
    RCC->APB2ENR |= (1U << 2) | (1U << 14);

    GPIOA->CRH &= ~((0xFU << 4) | (0xFU << 8));
    GPIOA->CRH |=  (0x0B << 4) | (0x04 << 8);

    USART1->BRR = BAUD_9600_BRR;
    USART1->CR1 = (1U << 13) | (1U << 3) | (1U << 2);
}

void DRV_USART1_SendChar(char c)
{
    while (!(USART1->SR & (1U << 7)));
    USART1->DR = c;
}

char DRV_USART1_ReceiveChar(void)
{
    while (!(USART1->SR & (1U << 5)));
    return (char)(USART1->DR & 0xFF);
}

uint8_t DRV_USART1_CharAvailable(void)
{
    return (USART1->SR & (1U << 5)) ? 1 : 0;
}
