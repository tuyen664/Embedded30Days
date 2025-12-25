#include "uart_if.h"
#include "bsp_uart.h"

void Console_Init(void)
{
    BSP_UART_Init();
}

void Console_Print(const char *s)
{
    BSP_UART_SendString(s);
}

uint8_t Console_CharAvailable(void)
{
    return BSP_UART_CharAvailable();
}

char Console_ReadChar(void)
{
    return BSP_UART_ReadChar();
}
