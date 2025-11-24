/**
 ******************************************************************************
 * @file    : main.c
 * @brief   : FreeRTOS Software Timer Demo (Register-level, No HAL)
 ******************************************************************************
 */

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

/* ================== DEFINES ================== */
#define LED_PIN        13
#define LED_ON()       (GPIOC->BRR = (1 << LED_PIN))
#define LED_OFF()      (GPIOC->BSRR = (1 << LED_PIN))
#define LED_TOGGLE()   (GPIOC->ODR ^= (1 << LED_PIN))

#define UART_BAUDRATE  9600

/* ================== FUNCTION PROTOTYPES ================== */
static void SystemClock_Config(void);
static void GPIO_Config(void);
static void UART1_Config(uint32_t baud);
static void UART1_SendChar(char c);
static void UART1_SendString(const char *s);
static void vTimerCallback_LED(TimerHandle_t xTimer);
static void vTimerCallback_UART(TimerHandle_t xTimer);
static void vTimerCallback_UART2(TimerHandle_t xTimer);

/* ================== GLOBALS ================== */
TimerHandle_t xTimer_LED, xTimer_UART , xTimer_UART2 ;

/* ================== MAIN ================== */
int main(void)
{
    SystemClock_Config();
    GPIO_Config();
    UART1_Config(UART_BAUDRATE);

    UART1_SendString("\r\n[SYS] FreeRTOS Timer Demo Start\r\n");

    /* Create timers */
    xTimer_LED  = xTimerCreate("LED_Timer",  pdMS_TO_TICKS(1000), pdTRUE,  0, vTimerCallback_LED);
    xTimer_UART = xTimerCreate("UART_Timer", pdMS_TO_TICKS(1000), pdTRUE,  0, vTimerCallback_UART);
	  xTimer_UART2 = xTimerCreate("UART2_Timer", pdMS_TO_TICKS(1000), pdTRUE,  0, vTimerCallback_UART2);


    if (xTimer_LED != NULL && xTimer_UART != NULL && xTimer_UART2 != NULL)
    {
        xTimerStart(xTimer_LED, 0);
        xTimerStart(xTimer_UART, 0);
			  xTimerStart(xTimer_UART2, 0);
    }
    else
    {
        UART1_SendString("[ERR] Timer creation failed!\r\n");
        while (1);
    }

    vTaskStartScheduler();
    while (1);
}

/* ================== CLOCK CONFIG ================== */
static void SystemClock_Config(void)
{
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;
    RCC->CFGR |= RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

/* ================== GPIO CONFIG ================== */
static void GPIO_Config(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    GPIOC->CRH &= ~(0xF << 20);
    GPIOC->CRH |=  (0x3 << 20);
    LED_ON();
}

/* ================== UART CONFIG ================== */
static void UART1_Config(uint32_t baud)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;

    // TX: PA9
    GPIOA->CRH &= ~(0xF << 4);
    GPIOA->CRH |=  (0xB << 4);

    uint32_t pclk = 72000000;
    USART1->BRR = pclk / baud;
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

/* ================== TIMER CALLBACKS ================== */
static void vTimerCallback_LED(TimerHandle_t xTimer)
{   
	  (void)xTimer;
    LED_TOGGLE();
}

static void vTimerCallback_UART(TimerHandle_t xTimer)
{   
	  (void)xTimer;
    UART1_SendString("[UART] Timer event: system alive!\r\n");
}

static void vTimerCallback_UART2(TimerHandle_t xTimer)
{   
	  (void)xTimer;
    UART1_SendString("[TEST] YESSSSSSSSSSSSS!\r\n");
}
