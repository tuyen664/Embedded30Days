/**
 ******************************************************************************
 * @file    : main.c
 * @brief   : FreeRTOS Queue + Semaphore Demo (Register-level, no HAL)
 ******************************************************************************
 */

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* ================== DEFINES ================== */
#define LED_PIN        13
#define BUTTON_PIN     1   // PA1
#define LED_ON()       (GPIOC->BRR = (1 << LED_PIN))
#define LED_OFF()      (GPIOC->BSRR = (1 << LED_PIN))
#define LED_TOGGLE()   (GPIOC->ODR ^= (1 << LED_PIN))

#define QUEUE_LENGTH   5
#define ITEM_SIZE      sizeof(uint8_t)

/* ================== FUNCTION PROTOTYPES ================== */
static void SystemClock_Config(void);
static void GPIO_Config(void);
static void UART1_Config(uint32_t baudrate);
static void UART1_SendChar(char c);
static void UART1_SendString(const char *s);
static void Task_LED(void *pvParameters);
static void Task_Button(void *pvParameters);
static void Task_Button_UART(void *pvParameters);
static void Task_Semaphore(void *pvParameters);

/* ================== GLOBALS ================== */
static QueueHandle_t xQueueButton = NULL;
static SemaphoreHandle_t xUART_Semaphore = NULL;

/* ================== MAIN ================== */
int main(void)
{
    SystemClock_Config();
    GPIO_Config();
    UART1_Config(9600);

    UART1_SendString("\r\nSystem Init OK\r\n");

    /* Create Queue and Semaphore */
    xQueueButton = xQueueCreate(QUEUE_LENGTH, ITEM_SIZE);
    xUART_Semaphore = xSemaphoreCreateMutex();

    if (xQueueButton == NULL || xUART_Semaphore == NULL)
    {
        UART1_SendString("Resource creation failed!\r\n");
        while (1);
    }

    /* Create Tasks */
    xTaskCreate(Task_LED, "LED", 128, NULL, 1, NULL);
    xTaskCreate(Task_Button, "Button", 128, NULL, 2, NULL);
    xTaskCreate(Task_Button_UART, "UART", 256, NULL, 1, NULL);
    xTaskCreate(Task_Semaphore, "Alive", 256, NULL, 1, NULL);

    vTaskStartScheduler();
    while (1);
}

/* ================== CLOCK CONFIG ================== */
static void SystemClock_Config(void)
{
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;
    RCC->CFGR |= RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9; // 8MHz * 9 = 72MHz
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2; // APB1 = 36MHz
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

/* ================== GPIO CONFIG ================== */
static void GPIO_Config(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;

    /* LED PC13 */
    GPIOC->CRH &= ~(0xF << 20);
    GPIOC->CRH |=  (0x3 << 20); // Output push-pull 50MHz
    LED_OFF();

    /* Button PA1 (Input Pull-up) */
    GPIOA->CRL &= ~(0xF << 4);
    GPIOA->CRL |=  (0x8 << 4);
    GPIOA->BSRR = (1 << BUTTON_PIN);
}

/* ================== UART CONFIG ================== */
static void UART1_Config(uint32_t baudrate)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    // PA9 TX
    GPIOA->CRH &= ~(0xF << 4);
    GPIOA->CRH |=  (0xB << 4);

    // PA10 RX
    GPIOA->CRH &= ~(0xF << 8);
    GPIOA->CRH |=  (0x4 << 8);

    uint32_t pclk = 72000000;
    USART1->BRR = pclk / baudrate;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

static void UART1_SendChar(char c)
{
    while (!(USART1->SR & USART_SR_TXE));
    USART1->DR = c;
}

static void UART1_SendString(const char *s)
{
    while (*s)
        UART1_SendChar(*s++);
}

/* ================== TASKS ================== */
static void Task_LED(void *pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        LED_TOGGLE();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void Task_Button(void *pvParameters)
{
	(void)pvParameters;
    uint8_t msg = 1;
    uint8_t prev = 1;
    const TickType_t debounce = pdMS_TO_TICKS(50);

    for (;;)
    {
        uint8_t curr = (GPIOA->IDR & (1 << BUTTON_PIN)) ? 1 : 0;
        if (prev == 1 && curr == 0)
        {
            xQueueSend(xQueueButton, &msg, 0); // 0 - no wait
        }
        prev = curr;
        vTaskDelay(debounce);
    }
}

static void Task_Button_UART(void *pvParameters)
{
	(void)pvParameters;
    uint8_t recv;
    for (;;)
    {
        if (xQueueReceive(xQueueButton, &recv, portMAX_DELAY) == pdPASS)
        {
       
            if (xSemaphoreTake(xUART_Semaphore, portMAX_DELAY) == pdTRUE)
            {
                UART1_SendString("[UART] Button Pressed!\r\n");
                LED_TOGGLE();
                xSemaphoreGive(xUART_Semaphore);
            }
        }
    }
}

static void Task_Semaphore(void *pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        if (xSemaphoreTake(xUART_Semaphore, portMAX_DELAY) == pdTRUE)
        {
            UART1_SendString("[SYS] System alive!\r\n");
            xSemaphoreGive(xUART_Semaphore);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
