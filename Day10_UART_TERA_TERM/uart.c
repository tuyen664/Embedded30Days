#include "uart.h"
#include "systick.h"

void USART1_Init(void)
{
    RCC->APB2ENR |= (1 << 2) | (1 << 14); // enable GPIOA and USART
    GPIOA->CRH &= ~((0xF << 4) | (0xF << 8)); 
    GPIOA->CRH |= (0x0B << 4) | (0x04 << 8); // PA9(TX- AFPP) , PA10(RX-Input floating)
    USART1->BRR = SystemCoreClock / 9600;
    USART1->CR1 = (1 << 13) | (1 << 3) | (1 << 2); // enable  UE,TE,RE
}

void USART1_SendChar(char c)
{
    while (!(USART1->SR & (1 << 7)));
    USART1->DR = c;
}

void USART1_SendString(const char *s)
{
    while (*s)
    {
        if (*s == '\n') USART1_SendChar('\r');
        USART1_SendChar(*s++);
    }
}

int USART1_ReceiveChar(uint32_t timeout_ms)
{
    uint32_t start = GetTick();
    while (!(USART1->SR & (1 << 5)))
    {
        if ((GetTick() - start) >= timeout_ms) return -1;
    }
   
		char c = (char)(USART1->DR & 0xFF);

    // chuyen chu thuong thanh hoa
    if (c >= 'a' && c <= 'z')
        c -= 32;

    return (int)c;
}
