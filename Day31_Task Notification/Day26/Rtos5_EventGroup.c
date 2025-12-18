/**
 ******************************************************************************
 * @file    : main.c
 * @brief   : FreeRTOS Demo – UART + LED + ADC + TIM2 + EXTI (EventGroup)
 ******************************************************************************
 */

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
//#include "event_groups.h"
#include <stdio.h>

/* ================== DEFINES ================== */
#define LED_PIN        13
#define LED_TOGGLE()   (GPIOC->ODR ^= (1 << LED_PIN))

#define EVENT_BIT_TIMER   (1 << 0)
#define EVENT_BIT_BUTTON  (1 << 1)

/* ================== FUNCTION PROTOTYPES ================== */
static void SystemClock_Config(void);
static void GPIO_Config(void);
static void UART1_Config(uint32_t baudrate);
static void UART1_SendChar(char c);
static void UART1_SendString(const char *s);
static void ADC1_Config(void);
static uint16_t readADC(uint8_t channel);
static uint16_t readADC_Avg(uint8_t channel, uint8_t samples);
static void TIM2_Config(void);
static void EXTI1_Config(void);
static void delay_ms(uint32_t ms);

/* ================== TASKS ================== */
static void Task_ADC(void *pvParameters);
static void Task_Status(void *pvParameters);
static void Task_EventHandler(void *pvParameters);

/* ================== GLOBALS ================== */
static SemaphoreHandle_t xUART_Mutex = NULL;
// static EventGroupHandle_t xEventGroup = NULL;
static TaskHandle_t xEventTaskHandle = NULL;


/* ================== MAIN ================== */
int main(void)
{
    SystemClock_Config();
    GPIO_Config();
    UART1_Config(9600);
    ADC1_Config();

    UART1_SendString("\r\n[SYS] Start FreeRTOS EventGroup Demo\r\n");

    /* Create Mutex & EventGroup */
    xUART_Mutex = xSemaphoreCreateMutex();
   // xEventGroup = xEventGroupCreate();

   

    TIM2_Config();  // Timer ngat 1Hz
    EXTI1_Config(); // Ngat nut nhan PA1

    /* Create Tasks */
    xTaskCreate(Task_Status,      "Status", 256, NULL, 1, NULL);
    xTaskCreate(Task_ADC,         "ADC",    384, NULL, 1, NULL);
    xTaskCreate(Task_EventHandler,"Event",  256, NULL, 2, &xEventTaskHandle);

		if (xUART_Mutex == NULL || xEventTaskHandle == NULL)
    {
        UART1_SendString("[ERR] Mutex/EventGroup creation failed!\r\n");
        while (1);
    } 
    UART1_SendString("[SYS] System Init OK\r\n");

    /* Start Scheduler */
    vTaskStartScheduler();

    UART1_SendString("[ERR] Scheduler failed!\r\n");
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
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPAEN;

    // PC13 (LED)
    GPIOC->CRH &= ~(0xF << 20);
    GPIOC->CRH |=  (0x3 << 20);

    
    GPIOA->CRL &= ~(0xF << 4);
    GPIOA->CRL |=  (0x8 << 4); // Input with pull-up/down
    GPIOA->ODR |= (1 << 1); // chon pull up

}

/* ================== UART CONFIG ================== */
static void UART1_Config(uint32_t baudrate)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_IOPAEN;

    // PA9 (TX)
    GPIOA->CRH &= ~(0xF << 4);
    GPIOA->CRH |=  (0xB << 4);

    // PA10 (RX)
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

/* ================== TIMER2 CONFIG ================== */
static void TIM2_Config(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    TIM2->PSC = 7200 - 1;     // 10kHz
    TIM2->ARR = 10000 - 1;    // 1Hz
    TIM2->DIER |= TIM_DIER_UIE;
    TIM2->CR1  |= TIM_CR1_CEN;

    NVIC_SetPriority(TIM2_IRQn, 8);
    NVIC_EnableIRQ(TIM2_IRQn);
}

/* ================== EXTI1 CONFIG (PA1 BUTTON) ================== */
static void EXTI1_Config(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    AFIO->EXTICR[0] &= ~(0xF << 4); // PA1
	AFIO->EXTICR[0] |=  (0x0 << 4); // PA1 -> EXTI1
    EXTI->IMR  |= (1 << 1);
    EXTI->FTSR |= (1 << 1); // Falling edge trigger
	EXTI->RTSR &= ~(1 << 1);                 // Disable rising edge

    NVIC_SetPriority(EXTI1_IRQn, 8);
    NVIC_EnableIRQ(EXTI1_IRQn);
}

/* ================== ISR: TIMER2 ================== */
void TIM2_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (TIM2->SR & TIM_SR_UIF)
    {
        TIM2->SR &= ~TIM_SR_UIF;
        //xEventGroupSetBitsFromISR(xEventGroup, EVENT_BIT_TIMER, &xHigherPriorityTaskWoken);
			xTaskNotifyFromISR(
          xEventTaskHandle,
          EVENT_BIT_TIMER,
          eSetBits,
          &xHigherPriorityTaskWoken
          );
			
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* ================== ISR: EXTI1 ================== */
void EXTI1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (EXTI->PR & (1 << 1)) // chi doc
    {
        EXTI->PR = (1 << 1); // Clear pending flag , ghi |= khong nen , ghi 0 là giu nguyen , 1 la xoa
       // xEventGroupSetBitsFromISR(xEventGroup, EVENT_BIT_BUTTON, &xHigherPriorityTaskWoken);
       xTaskNotifyFromISR(
           xEventTaskHandle,
           EVENT_BIT_BUTTON,
           eSetBits,
           &xHigherPriorityTaskWoken
           );

			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* ================== TASK: EVENT HANDLER ================== */
/* static void Task_EventHandler(void *pvParameters)
{
	(void) pvParameters ;
    uint32_t ulNotifyValue;

    for (;;)
    {
        uxBits = xEventGroupWaitBits(
                    xEventGroup,
                    EVENT_BIT_TIMER | EVENT_BIT_BUTTON,
                    pdTRUE,    // Clear bits after received
                    pdFALSE,   // Wait any bit (OR)
                    portMAX_DELAY   ); 
           
			
        if (uxBits & EVENT_BIT_TIMER)
        {
          
            if (xSemaphoreTake(xUART_Mutex, pdMS_TO_TICKS(50)) == pdTRUE)
             {
                UART1_SendString("[EVT] Timer tick\r\n");
                xSemaphoreGive(xUART_Mutex);
             }
         }

        if (uxBits & EVENT_BIT_BUTTON)
        {
			LED_TOGGLE();
			
            if (xSemaphoreTake(xUART_Mutex, pdMS_TO_TICKS(50)) == pdTRUE)
              {
							  
                UART1_SendString("[EVT] Button pressed\r\n");
                xSemaphoreGive(xUART_Mutex);
              }
        }
    }
}
*/

static void Task_EventHandler(void *pvParameters)
{
    (void)pvParameters;
    uint32_t ulNotifyValue;

    for (;;)
    {
        xTaskNotifyWait(
            0x00,       // Khong clear truoc
            EVENT_BIT_TIMER | EVENT_BIT_BUTTON,
            &ulNotifyValue,
            portMAX_DELAY
        );

        if (ulNotifyValue & EVENT_BIT_TIMER)
        {
            if (xSemaphoreTake(xUART_Mutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                UART1_SendString("[EVT] Timer tick\r\n");
                xSemaphoreGive(xUART_Mutex);
            }
        }

        if (ulNotifyValue & EVENT_BIT_BUTTON)
        {
            LED_TOGGLE();
            if (xSemaphoreTake(xUART_Mutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                UART1_SendString("[EVT] Button pressed\r\n");
                xSemaphoreGive(xUART_Mutex);
            }
        }
    }
}



/* ================== TASK: STATUS ================== */
static void Task_Status(void *pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        if (xSemaphoreTake(xUART_Mutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            UART1_SendString("[STATUS] System OK\r\n");
            xSemaphoreGive(xUART_Mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ================== TASK: ADC ================== */
static void Task_ADC(void *pvParameters)
{
    (void)pvParameters;
    uint16_t adcValue;
    char buffer[32];

    for (;;)
    {
        adcValue = readADC_Avg(0, 8); // PA0
        if (xSemaphoreTake(xUART_Mutex, portMAX_DELAY) == pdTRUE)
        {
            snprintf(buffer, sizeof(buffer), "[ADC] Value = %u\r\n", adcValue);
            UART1_SendString(buffer);
            xSemaphoreGive(xUART_Mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ================== ADC CONFIG ================== */
static void ADC1_Config(void)
{
    RCC->APB2ENR |= (1U << 2) | (1U << 9);
    RCC->CFGR &= ~(0x3U << 14);
    RCC->CFGR |=  (0x2U << 14); // PCLK2 / 6 = 12MHz

    GPIOA->CRL &= ~(0xFU << 0);
    ADC1->SMPR2 |= (0x7U << 0);

    ADC1->CR2 |= (1U << 0);
    delay_ms(1);
    ADC1->CR2 |= (1U << 3); while (ADC1->CR2 & (1U << 3));
    ADC1->CR2 |= (1U << 2); while (ADC1->CR2 & (1U << 2));
    ADC1->CR2 |= (1U << 1);
    ADC1->CR2 |= (1U << 0);
    ADC1->CR2 |= (1U << 22);
}

static uint16_t readADC(uint8_t channel)
{
    ADC1->SQR3 = channel;
    ADC1->CR2 |= (1U << 22);
    while (!(ADC1->SR & (1U << 1)));
    ADC1->SR &= ~(1U << 1);
    return ADC1->DR;
}

static uint16_t readADC_Avg(uint8_t channel, uint8_t samples)
{
    uint32_t sum = 0;
    for (uint8_t i = 0; i < samples; i++)
    {
        sum += readADC(channel);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    return (uint16_t)(sum / samples);
}

/* ================== SIMPLE DELAY ================== */
static void delay_ms(uint32_t ms)
{
    for (volatile uint32_t i = 0; i < ms * 8000; i++);
}
