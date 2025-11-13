#ifndef UART_H
#define UART_H
#include "stm32f10x.h"
#include <stdint.h>

void USART1_Init(void);
void USART1_SendChar(char c);
void USART1_SendString(const char *s);
int  USART1_ReceiveChar(uint32_t timeout_ms);

#endif
