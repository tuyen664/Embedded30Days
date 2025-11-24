/**
 ******************************************************************************
 * @file    : main.c
 * @brief   : FreeRTOS Mutex Demo – Shared Resource Protection (UART)
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
static uint16_t readADC_Avg(uint8_t channel , uint8_t samples);
static void Task_ADC(void *pvParameters);

static void delay_ms(uint32_t ms);

static void Task_Sensor(void *pvParameters);
static void Task_Status(void *pvParameters);
static void Task_Log(void *pvParameters);

/* ================== GLOBALS ================== */
static SemaphoreHandle_t xUART_Mutex = NULL;

/* ================== MAIN ================== */
int main(void)
{
    SystemClock_Config();
    GPIO_Config();
    UART1_Config(9600);
	  ADC1_Config();

    UART1_SendString("\r\n[SYS] System Init OK\r\n");

    /* Create Mutex */
    xUART_Mutex = xSemaphoreCreateMutex();

    if (xUART_Mutex == NULL)
    {
        UART1_SendString("[ERR] Mutex creation failed!\r\n");
        while (1);
    }

    /* Create Tasks */
    xTaskCreate(Task_Sensor, "Sensor", 256, NULL, 1, NULL);
    xTaskCreate(Task_Status, "Status", 256, NULL, 1, NULL);
    xTaskCreate(Task_Log,    "Log",    256, NULL, 1, NULL);
		xTaskCreate(Task_ADC, "ADC", 384, NULL, 1, NULL);

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
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    /* LED PC13 Output Push-Pull */
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

/* ================== TASKS ================== */
static void Task_Sensor(void *pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        if (xSemaphoreTake(xUART_Mutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            UART1_SendString("[SENSOR] Reading data...\r\n");
            xSemaphoreGive(xUART_Mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(800));
    }
}

static void Task_Status(void *pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        if (xSemaphoreTake(xUART_Mutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            UART1_SendString("[STATUS] System OK\r\n");
            LED_TOGGLE();
            xSemaphoreGive(xUART_Mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void Task_Log(void *pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        if (xSemaphoreTake(xUART_Mutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            UART1_SendString("[LOG] Event captured!\r\n");
            xSemaphoreGive(xUART_Mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

static void Task_ADC(void *pvParameters)
{
    (void)pvParameters;
    uint16_t adcValue;
    char buffer[32];

    for (;;)
    {
        adcValue = readADC_Avg(0, 8); // doc trung binh kenh PA0
        if (xSemaphoreTake(xUART_Mutex, portMAX_DELAY) == pdTRUE)
        {
            snprintf(buffer,sizeof(buffer), "[ADC] Value = %u\r\n", adcValue);
            UART1_SendString(buffer);
            xSemaphoreGive(xUART_Mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}





static void ADC1_Config(void)
{
    RCC->APB2ENR |= (1U << 2) | (1U << 9); // GPIOA + ADC1 clock
    RCC->CFGR &= ~(0x3U << 14);
    RCC->CFGR |=  (0x2U << 14); // PCLK2 / 6 = 12MHz for ADC

    // PA0, PA1, PA2 -> Analog mode
    GPIOA->CRL &= ~((0xFU << 0) | (0xFU << 4) | (0xFU << 8));

    // Sample time selection ->  239.5 ADC cycles
    ADC1->SMPR2 |= (0x7U << 0) | (0x7U << 3) | (0x7U << 6);

    // ADC calibration
    ADC1->CR2 |= (1U << 0); // ADON first time
    delay_ms(1);
    ADC1->CR2 |= (1U << 3); while (ADC1->CR2 & (1U << 3)); // RSTcal -> Reset Cal
    ADC1->CR2 |= (1U << 2); while (ADC1->CR2 & (1U << 2)); // Calibrate
	
	  ADC1->CR2 |= (1U << 1);               // CONT mode
   
	  ADC1->CR2 |= (1U << 0); // Bat ADC lan 2 sau khi calibrate
    ADC1->CR2 |= (1U << 22);              // SWSTART (start conversion)
}
static uint16_t readADC(uint8_t channel)
{
	  ADC1->SQR3 = channel;          // chon channel
	  ADC1->CR2 |= (1U << 22);       // SWSTART: start convert
    while (!(ADC1->SR & (1U << 1))); // wait EOC
	
	  ADC1->SR &= ~(1U << 1);          // xóa EOC (truong hop ADC liên tuc)
    return ADC1->DR;
}

static uint16_t readADC_Avg(uint8_t channel , uint8_t samples)
{
    uint32_t sum = 0;
    for (uint8_t i = 0; i < samples; i++)
    {
        sum += readADC(channel);
			  vTaskDelay(pdMS_TO_TICKS(2));;
    }
    return (uint16_t)(sum / samples);
}

static void delay_ms(uint32_t ms)
{
    for (volatile uint32_t i = 0; i < ms * 8000; i++);
}