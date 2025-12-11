#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include <stdio.h>

#define LED_PIN            13
#define LED_TOGGLE()    (GPIOC->ODR ^= (1<<LED_PIN))
#define LED_ON()        (GPIOC->BRR = (1<<LED_PIN))
#define LED_OFF()       (GPIOC->BSRR = (1<<LED_PIN))

#define ADC_CHANNEL   0     // PA0
#define PWM_PIN       9     // PB9
#define UART_BAUD     9600

static void System_Init(void);
static void SystemClock_Config(void);
static void GPIO_Config(void);
static void UART1_Config(uint32_t baudrate);
static void UART1_SendString(const char *s);
static void UART1_SendChar(char c);
static void TIM4_PWM_Init(void);
static void ADC1_Config(void);
static uint16_t readADC(uint8_t channel);
static uint16_t readADC_Avg(uint8_t channel , uint8_t samples);

static void Task_ADC_Read(void *pv);
static void Task_PWM_Control(void *pv);
static void Task_UART_Command(void *pv);
static void vTimerCallback_Monitor(TimerHandle_t xTimer);

static void delay_ms( uint8_t ms);

QueueHandle_t xQueueADC = NULL;
SemaphoreHandle_t xUART_Sem = NULL;
TimerHandle_t xTimerMonitor;


int main(void)
{
	System_Init();
	UART1_SendString("\r\n[SYS] Smart Sensor RTOS Project Start\r\n");
	
	xQueueADC = xQueueCreate(10, sizeof(uint16_t));
	xUART_Sem = xSemaphoreCreateMutex();
	xTimerMonitor = xTimerCreate("Monitor", pdMS_TO_TICKS(3000), pdTRUE, 0, vTimerCallback_Monitor);
	
	if (xQueueADC == NULL || xUART_Sem == NULL || xTimerMonitor == NULL)
	{
		UART1_SendString("[ERR] Resource creation failed!\r\n");
		while (1);
	}
	
	xTaskCreate(Task_ADC_Read, "ADC_Read", 256, NULL, 2, NULL);
	xTaskCreate(Task_PWM_Control, "PWM_Ctrl", 256, NULL, 2, NULL);
	xTaskCreate(Task_UART_Command, "UART_Cmd", 256, NULL, 1, NULL);

	xTimerStart(xTimerMonitor, 0);
	
	vTaskStartScheduler();
	
  while (1);
}



static void Task_ADC_Read(void *pv)
{
	(void)pv;
	uint16_t adc_send = 0 ;
	for(;;)
	{
		adc_send = readADC_Avg (ADC_CHANNEL,5);
		xQueueSend(xQueueADC, &adc_send, 0);
		vTaskDelay(pdMS_TO_TICKS(200));	
	}
}

static void Task_PWM_Control(void *pv)
{
	(void)pv;
	uint16_t adc_recv;
	char buffer[32];
	for (;;)
	{
		if (xQueueReceive(xQueueADC, &adc_recv, portMAX_DELAY) == pdPASS)
		{
			uint16_t duty =(adc_recv * 1000 / 4095);
			TIM4->CCR4 = duty;
			TIM4->EGR |= 1;
			if (xSemaphoreTake(xUART_Sem, portMAX_DELAY) == pdTRUE)
			{
				snprintf(buffer,sizeof(buffer), "[DUTY] Value = %u\r\n", duty);
				UART1_SendString(buffer);
			
				xSemaphoreGive(xUART_Sem);
			}
		}
	}
}

static void Task_UART_Command(void *pv)
{
	(void)pv;
	for (;;)
	{
		if (xSemaphoreTake(xUART_Sem, portMAX_DELAY) == pdTRUE)
		{
			 UART1_SendString("[UART] ADC Readings Active\r\n");
			 xSemaphoreGive(xUART_Sem);
		}
		vTaskDelay(pdMS_TO_TICKS(5000));
	}

}

static void vTimerCallback_Monitor(TimerHandle_t xTimer)
{ 
	(void)xTimer;
	LED_TOGGLE();
	UART1_SendString("[SYS] Alive\r\n");
}


static void System_Init(void)
{
	SystemClock_Config();
	GPIO_Config();
	UART1_Config( UART_BAUD );
	TIM4_PWM_Init();
	ADC1_Config();

}


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

static void GPIO_Config(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN  ;

    /* LED PC13 */
    GPIOC->CRH &= ~(0xF << 20);
    GPIOC->CRH |=  (0x3 << 20); // Output push-pull 50MHz
    LED_ON();
}
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

static void ADC1_Config(void)
{
    RCC->APB2ENR |= (1U << 2) | (1U << 9); // GPIOA + ADC1 clock
    RCC->CFGR &= ~(0x3U << 14);
    RCC->CFGR |=  (0x2U << 14); // PCLK2 / 6 = 12MHz for ADC

    // PA0, PA1, PA2 -> Analog mode
    GPIOA->CRL &= ~((0xFU << 0) | (0xFU << 4) | (0xFU << 8));

    // Sample time selection ->  239.5 ADC cycles
    ADC1->SMPR2 |= (0x7U << 0) | (0x7U << 3) | (0x7U << 6);
	
    ADC1->CR2 |= (1U << 1);               // CONT mode

    // ADC calibration
    ADC1->CR2 |= (1U << 0); // ADON first time
    delay_ms(1);
    ADC1->CR2 |= (1U << 3); while (ADC1->CR2 & (1U << 3)); // RSTcal -> Reset Cal
    ADC1->CR2 |= (1U << 2); while (ADC1->CR2 & (1U << 2)); // Calibrate
   
	  // ADC1->CR2 |= (1U << 0); // Bat ADC lan 2 sau khi calibrate
    // ADC1->CR2 |= (1U << 22);              // SWSTART (start conversion)
}

static void TIM4_PWM_Init(void)
{
    // Enable clocks for GPIOB and TIM4
    RCC->APB2ENR |= (1U << 3); // GPIOB clock enable
    RCC->APB1ENR |= (1U << 2); // TIM4 clock enable
	
    RCC->APB2ENR |= (1U << 0); // Enable AFIO clock

    // Configure PB9 -TIM4_CH4 (AF Push-Pull)
    GPIOB->CRH &= ~(0xFU << 4);
    GPIOB->CRH |= (0x0B << 4); // Output 50MHz, AF Push-Pull

    // Timer configuration

    TIM4->PSC = 71;
    TIM4->ARR = 1000-1; // 1kHz -> cho led

    TIM4->CCR4 = 0; // start LED_OFF 
    TIM4->CCMR2 |= (6U << 12) | (1U << 11);  // PWM mode 1 + preload enable
    TIM4->CCER  = (1U << 12);              // Enable output on CH4
    TIM4->CR1   = (1U << 7) | (1U << 0);   // ARPE + Counter enable
		
	TIM4->EGR |= 1; // Generate update event
	delay_ms(20);
}

static uint16_t readADC(uint8_t channel)
{
	ADC1->SQR3 = channel;          // chon channel
    ADC1->CR2 |= (1U << 22);       // SWSTART: start convert
    while (!(ADC1->SR & (1U << 1)));
	
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

static void delay_ms( uint8_t ms)
{
	for ( uint32_t i = 0 ; i < 8000*ms ; i++);
}






















































