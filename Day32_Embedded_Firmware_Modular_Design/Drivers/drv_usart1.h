#ifndef DRV_USART1_H
#define DRV_USART1_H

#include <stdint.h>

void DRV_USART1_Init(void);
void DRV_USART1_SendChar(char c);
char DRV_USART1_ReceiveChar(void);
uint8_t DRV_USART1_CharAvailable(void);

#endif
