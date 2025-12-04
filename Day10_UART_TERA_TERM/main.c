#include "stm32f10x.h"
#include "led.h"
#include "uart.h"
#include "systick.h"
#include <string.h>

int main(void)
{
    SysTick_Init();
    LED_Init();
	USART1_Init();
    USART1_SendString("UART Command Mode Ready\r\n");

    uint8_t blinkMode = 0;
    uint32_t blinkLast = GetTick();
    char buffer[32];
    uint8_t idx = 0;

    while (1)
    {
        // blink (non-blocking)
        if (blinkMode && Delay_Elapsed(blinkLast, 500))
        {
            LED_TOGGLE();
            blinkLast = GetTick();
        }

        // UART polling
        int rc = USART1_ReceiveChar(10);
        if (rc < 0) continue;

        char c = (char)rc;
        if (c == '\r' || c == '\n')
        {
            if (idx == 0) continue;
            buffer[idx] = '\0'; idx = 0;

            if (strcmp(buffer, "ON") == 0)      
			{
				blinkMode = 0; LED_ON();  USART1_SendString("LED ON\r\n"); 
			}
            else if (strcmp(buffer, "OFF") == 0)
			{
				blinkMode = 0; LED_OFF(); USART1_SendString("LED OFF\r\n"); 
			}
            else if (strcmp(buffer, "BLINK") == 0)
			{
				blinkMode = 1; USART1_SendString("LED BLINK\r\n"); 
			}
            else if (strcmp(buffer, "STATUS") == 0)
			{
                if (IS_LED_ON()) USART1_SendString("LED IS ON\r\n");
                else USART1_SendString("LED IS OFF\r\n");
            }
            else USART1_SendString("UNKNOWN CMD\r\n");
            memset(buffer, 0, sizeof(buffer));
        }
        else if (idx < sizeof(buffer) - 1)  buffer[idx++] = c;
    }
}
