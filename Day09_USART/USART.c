#include "stm32f10x.h"

void USART1_Init(void);
void USART1_SendChar(char c);
void USART1_SendString(char *str);

int main(void)
{
    USART1_Init();
    while(1)
    {
        USART1_SendString("HELLO!\r\n");
        for(int i = 0; i < 1000000; i++);  // delay
    }
}

void USART1_Init(void)
{
    RCC->APB2ENR |= (1<<2) | (1<<14);   // Enable GPIOA and USART1

    GPIOA->CRH &= ~((0xF<<4) | (0xF<<8));
    GPIOA->CRH |=  (0x0B<<4);           // PA9 (TX): AF Push-Pull, 50 MHz
    GPIOA->CRH |=  (0x04<<8);           // PA10 (RX): Input floating mode

  
    USART1->BRR = 0x1D4C;          //  baudrate = 9600 
	
	// Enable USART, Transmitter, Receiver (UE=1, TE=1, RE=1)
    USART1->CR1 = (1<<13) | (1<<3) | (1<<2);  
}

void USART1_SendChar(char c)
{
    while(!(USART1->SR & (1<<7))); // Wait TXE = 1
    USART1->DR = c;               // Send character
}

void USART1_SendString(char *str)
{
    while(*str)
        USART1_SendChar(*str++);
}
