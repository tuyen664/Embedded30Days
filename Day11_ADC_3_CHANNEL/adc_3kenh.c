#include "stm32f10x.h"
#include <stdio.h>

/* ===================== CONFIG MACROS ===================== */
#define ADC_CH1            0U          // PA0
#define ADC_CH2            1U          // PA1
#define ADC_CH3            2U          // PA2
#define ADC_SAMPLE         5          // lay trung binh 5 sample
#define BAUD               0x1D4C
#define VREF               3.3f
#define ADC_RESOLUTION     4095.0f



/* ===================== FUNCTION PROTOTYPES ===================== */
static void UART1_Init(void);
static void UART1_SendChar(char c);
static void UART1_SendString(const char *s);

static void ADC1_Init(void);
static uint16_t ADC1_Read(uint8_t channel);
static uint16_t ADC1_ReadAverage(uint8_t channel, uint8_t sample_count);

static void SysTick_Init(void);
static void DelayMs(uint32_t ms);

/* ===================== GLOBAL ===================== */
static volatile uint32_t sysTickMs = 0;

/* ===================== MAIN ===================== */
int main(void)
{
    char msg[64];
    uint16_t val1, val2 , val3;
    float volt1, volt2 , volt3 ;

    SysTick_Init();
    UART1_Init();
    ADC1_Init();

    UART1_SendString("=== ADC Channel Demo ===\r\n");

    while (1)
    {
        val1 = ADC1_ReadAverage(ADC_CH1, ADC_SAMPLE);
        val2 = ADC1_ReadAverage(ADC_CH2, ADC_SAMPLE);
        val3 = ADC1_ReadAverage(ADC_CH3, ADC_SAMPLE);
        volt1 = (val1 * VREF) / ADC_RESOLUTION;
			  volt2 = (val2 * VREF) / ADC_RESOLUTION;
			  volt3 = (val3 * VREF) / ADC_RESOLUTION;

        sprintf(msg, "CH1=%u(%.2fV), CH2=%u(%.2fV), CH3=%u(%.2fV)\r\n",
                val1, volt1, val2, volt2, val3, volt3);
        UART1_SendString(msg);

        DelayMs(500); // 500ms delay
    }
}

/* ===================== SYSTICK ===================== */
static void SysTick_Init(void)
{
    SysTick->LOAD = (SystemCoreClock / 1000U) - 1U;  // 1ms tick
    SysTick->VAL  = 0U;
    SysTick->CTRL = 7U; // ENABLE | TICKINT | CLKSOURCE
}

void SysTick_Handler(void)
{
    sysTickMs++;
}

static void DelayMs(uint32_t ms)
{
    uint32_t start = sysTickMs;
    while ((sysTickMs - start) < ms);
}

/* ===================== UART ===================== */
static void UART1_Init(void)
{
    RCC->APB2ENR |= (1U << 0) | (1U << 2) | (1U << 14); // AFIO + GPIOA + USART1 clock enable

    // PA9 (TX) = AF push-pull, PA10 (RX) = floating input
    GPIOA->CRH &= ~((0xFU << 4) | (0xFU << 8));
    GPIOA->CRH |=  (0x0BU << 4) | (0x04U << 8);

    USART1->BRR =  BAUD ;
    USART1->CR1 = (1U << 13) | (1U << 3) | (1U << 2); // UE, TE, RE enable
}

static void UART1_SendChar(char c)
{
    while (!(USART1->SR & (1U << 7))); // Wait TXE
    USART1->DR = c;
}

static void UART1_SendString(const char *s)
{
    while (*s)
    {
        if (*s == '\n') UART1_SendChar('\r'); // Add CR for terminal
        UART1_SendChar(*s++);
    }
}

/* ===================== ADC ===================== */
static void ADC1_Init(void)
{
    RCC->APB2ENR |= (1U << 2) | (1U << 9); // Enable GPIOA and ADC1 clocks

    // Configure PA0 PA1,PA2 as analog input
    GPIOA->CRL &= ~((0xF << (4*ADC_CH1 )) | (0xF << (4* ADC_CH2 )) | (0xF << (4* ADC_CH3 )));

    // Power on the ADC
    ADC1->CR2 |= (1U << 0);  // ADON = 1
    DelayMs(1);
	  // ADC1->CR2 |= (1U << 0);   // Lan 2: start ADC (optional theo manual)
	
	  ADC1->CR2 |= (1U << 1);   // CONT = 1 (Continuous mode)

    
    // Enable software trigger mode
    ADC1->CR2 |= (1U << 20); // EXTTRIG = 1 , chi can bat khi dung trigger ngoai
	  ADC1->CR2 &= ~(0x7U << 17); // Clear EXTSEL
    ADC1->CR2 |= (7U << 17); // EXTSEL = 111 (SWSTART)
	
	// Reset calibration
    ADC1->CR2 |= (1U << 3);
    while (ADC1->CR2 & (1U << 3));

    // Start calibration
    ADC1->CR2 |= (1U << 2);  // CAL = 1
    while (ADC1->CR2 & (1U << 2)); // Wait until calibration is done (CAL = 0)
}


static uint16_t ADC1_Read(uint8_t channel)
{
    ADC1->SQR3 = channel;        // Select channel
    ADC1->CR2  |= (1U << 22);    // Start conversion (SWSTART)
    while (!(ADC1->SR & (1U << 1))); // Wait for EOC
    return (uint16_t)ADC1->DR;   // Read result
}

static uint16_t ADC1_ReadAverage(uint8_t channel, uint8_t sample_count)
{
    uint32_t sum = 0;
    for(uint8_t i=0;i<sample_count;i++)
    {
        sum += ADC1_Read(channel);
    }
    return (uint16_t)(sum / sample_count);
}
