/**
 ******************************************************************************
 * @file    : main.c
 * @brief   : FreeRTOS Demo – UART + LED + ADC + TIM2 ISR via Semaphore
 ******************************************************************************
 */

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>

/* ================== DEFINES ================== */
#define LED_PIN        13
#define LED_TOGGLE()   (GPIOC->ODR ^= (1 << LED_PIN))

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
static void delay_ms(uint32_t ms);

static void Task_ADC(void *pvParameters);
static void Task_Status(void *pvParameters);
static void Task_TimerHandler(void *pvParameters);

/* ================== GLOBALS ================== */
static SemaphoreHandle_t xUART_Mutex = NULL;
static SemaphoreHandle_t xTimerSemaphore = NULL;

/* ================== MAIN ================== */
int main(void)
{
    SystemClock_Config();
    GPIO_Config();
    UART1_Config(9600);
    ADC1_Config();

    UART1_SendString("\r\n[SYS] Start FreeRTOS demo\r\n");

    /* Create Mutex and Semaphore */
    xUART_Mutex = xSemaphoreCreateMutex();
    xTimerSemaphore = xSemaphoreCreateBinary();
	
	  TIM2_Config(); // Goi sau khoi tao xTimerSemaphore

    if (xUART_Mutex == NULL || xTimerSemaphore == NULL)
    {
        UART1_SendString("[ERR] Mutex/Semaphore creation failed!\r\n");
        while (1);
    }

    /* Create Tasks */
    xTaskCreate(Task_Status,       "Status", 256, NULL, 1, NULL);
    xTaskCreate(Task_ADC,          "ADC",    384, NULL, 1, NULL);
    xTaskCreate(Task_TimerHandler, "Timer",  256, NULL, 2, NULL);

    UART1_SendString("[SYS] System Init OK\r\n");

    /* Start Scheduler */
    vTaskStartScheduler();

    UART1_SendString("[ERR] Scheduler failed to start!\r\n");
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
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    /* PC13 as Output Push-Pull (LED) */
    GPIOC->CRH &= ~(0xF << 20);
    GPIOC->CRH |=  (0x3 << 20);
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
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;    // Bat clock cho TIM2
    TIM2->PSC = 7200 - 1;                  // 72 MHz / 7200 = 10 kHz (16 bit 0-65535)
    TIM2->ARR = 10000 - 1;                  // 10 kHz / 10000 = 1Hz -> 1s
    TIM2->DIER |= TIM_DIER_UIE;            // Cho phep ngat update
    TIM2->CR1  |= TIM_CR1_CEN;             // Bat timer

   
    NVIC_SetPriority(TIM2_IRQn, 8); // #define configMAX_SYSCALL_INTERRUPT_PRIORITY  128  -> moi ngat muon goi API FreeRTOS phai co priority >= 8
    NVIC_EnableIRQ(TIM2_IRQn);
}

/* ================== TIMER2 ISR ================== */
void TIM2_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE; // co Task nao co do uu tien cao hon 

    if (TIM2->SR & TIM_SR_UIF)
    {
        TIM2->SR &= ~TIM_SR_UIF;   // Xoa co ngat

        if (xTimerSemaphore != NULL)
            xSemaphoreGiveFromISR(xTimerSemaphore, &xHigherPriorityTaskWoken);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}


/* ================== TASK: TIMER HANDLER ================== */
static void Task_TimerHandler(void *pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        if (xSemaphoreTake(xTimerSemaphore, portMAX_DELAY) == pdTRUE)
        {
            LED_TOGGLE();

            if (xSemaphoreTake(xUART_Mutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                UART1_SendString("[TIM2] 1000ms tick\r\n");
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
        adcValue = readADC_Avg(0, 8); // Read averaged ADC channel 0 (PA0)

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
    RCC->APB2ENR |= (1U << 2) | (1U << 9); // Enable GPIOA + ADC1 clock
    RCC->CFGR &= ~(0x3U << 14);
    RCC->CFGR |=  (0x2U << 14); // PCLK2 / 6 = 12MHz for ADC

    // PA0, PA1, PA2 -> Analog mode
    GPIOA->CRL &= ~((0xFU << 0) | (0xFU << 4) | (0xFU << 8));

    // Sample time selection -> 239.5 ADC cycles
    ADC1->SMPR2 |= (0x7U << 0) | (0x7U << 3) | (0x7U << 6);

    // ADC calibration
    ADC1->CR2 |= (1U << 0); // ADON first time
    delay_ms(1);
    ADC1->CR2 |= (1U << 3); while (ADC1->CR2 & (1U << 3)); // RSTCAL
    ADC1->CR2 |= (1U << 2); while (ADC1->CR2 & (1U << 2)); // CAL
    ADC1->CR2 |= (1U << 1); // Continuous mode
    ADC1->CR2 |= (1U << 0); // ADON (2nd time)
    ADC1->CR2 |= (1U << 22); // SWSTART
}

static uint16_t readADC(uint8_t channel)
{
    ADC1->SQR3 = channel;          // Select channel
    ADC1->CR2 |= (1U << 22);       // SWSTART
    while (!(ADC1->SR & (1U << 1))); // wait EOC
    ADC1->SR &= ~(1U << 1);        // Clear EOC
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

/* ================== SIMPLE DELAY (for ADC Calib) ================== */
static void delay_ms(uint32_t ms)
{
    for (volatile uint32_t i = 0; i < ms * 8000; i++);
}
