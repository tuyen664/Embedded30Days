#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

/* =================== FUNCTION PROTOTYPES =================== */
static void Task_LED(void *pvParameters);
static void Task_UART(void *pvParameters);
static void GPIO_Config(void);
static void UART1_Init(uint32_t baudrate);
static void UART1_SendString(const char *str);

/* =================== MAIN FUNCTION =================== */
int main(void)
{
    SystemInit();          // Initialize system clock and vector table
    GPIO_Config();         // Configure LED pin (PC13)
    UART1_Init(9600);      // Initialize UART1 at 9600 baud

    // === Create 2 independent tasks ===
    xTaskCreate(Task_LED, "LED", 128, NULL, 1, NULL);
    xTaskCreate(Task_UART, "UART", 256, NULL, 1, NULL);

    vTaskStartScheduler(); // Start the FreeRTOS scheduler

    while (1) {}           // Should never reach here
}

/* =================== TASK: LED BLINK =================== */
static void Task_LED(void *pvParameters)
{
    (void) pvParameters;
    for (;;)
    {
        GPIOC->ODR ^= (1 << 13);          // Toggle LED on PC13
        vTaskDelay(pdMS_TO_TICKS(500));   // Delay 500ms (non-blocking)
    }
}

/* =================== TASK: UART SENDER =================== */
static void Task_UART(void *pvParameters)
{
    (void) pvParameters;
    for (;;)
    {
        UART1_SendString("Hello from FreeRTOS!\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));  // Send every 1 second
    }
}

/* =================== GPIO CONFIGURATION =================== */
static void GPIO_Config(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;   // Enable clock for Port C

    // PC13 -> Output push-pull, max speed 2MHz
    GPIOC->CRH &= ~(0xF << 20);
    GPIOC->CRH |=  (0x2 << 20);           // 0010: Output push-pull, 2MHz
    GPIOC->ODR |=  (1 << 13);             // LED OFF by default
}

/* =================== UART CONFIGURATION =================== */
static void UART1_Init(uint32_t baudrate)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    // === PA9 -> TX (AF push-pull 50MHz) ===
    GPIOA->CRH &= ~(0xF << 4);   // clear bits CNF9[1:0], MODE9[1:0]
    GPIOA->CRH |=  (0xB << 4);   // CNF9=10 (AF PP), MODE9=11 (50MHz)

    // === PA10 -> RX (input floating) ===
    GPIOA->CRH &= ~(0xF << 8);
    GPIOA->CRH |=  (0x4 << 8);   // CNF10=01 (floating input), MODE10=00

    uint32_t pclk = 72000000;
    USART1->BRR = pclk / baudrate;

    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;  // Enable TX, RX, USART
}

/* =================== UART SEND STRING =================== */
static void UART1_SendString(const char *str)
{
    while (*str)
    {
        while (!(USART1->SR & USART_SR_TXE)); // Wait until TX buffer empty
        USART1->DR = (*str++ & 0xFF);         // Send next character
    }
}
