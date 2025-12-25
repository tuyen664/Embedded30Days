#ifndef UART_IF_H
#define UART_IF_H

#include <stdint.h>

void Console_Init(void);
void Console_Print(const char *s);
uint8_t Console_CharAvailable(void);
char Console_ReadChar(void);

#endif
