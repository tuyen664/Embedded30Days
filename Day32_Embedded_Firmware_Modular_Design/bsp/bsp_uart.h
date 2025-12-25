#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>

void BSP_UART_Init(void);
void BSP_UART_SendString(const char *s);
uint8_t BSP_UART_CharAvailable(void);
char BSP_UART_ReadChar(void);

#endif
