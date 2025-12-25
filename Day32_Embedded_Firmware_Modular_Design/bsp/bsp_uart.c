#include "bsp_uart.h"
#include "drv_usart1.h"

void BSP_UART_Init(void)
{
    DRV_USART1_Init();
}

void BSP_UART_SendString(const char *s)
{
    while (*s)
    {
        DRV_USART1_SendChar(*s++);
    }
}

uint8_t BSP_UART_CharAvailable(void)
{
    return DRV_USART1_CharAvailable();
}

char BSP_UART_ReadChar(void)
{
    return DRV_USART1_ReceiveChar();
}
