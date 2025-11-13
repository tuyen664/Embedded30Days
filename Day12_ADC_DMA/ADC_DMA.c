#include "stm32f10x.h"
#include <stdio.h>

#define ADC_CHANNELS   3
#define ADC_CH1        0
#define ADC_CH2        1
#define ADC_CH3        2
#define VREF           3.3f
#define ADC_RES        4095.0f
#define BAUD_9600_BRR  0x1D4C   // 9600 bps @72MHz

static volatile uint16_t adc_buf[ADC_CHANNELS];

// ====== Function prototypes ======
static void UART1_Init(void);
static void UART1_SendChar(char c);
static void UART1_SendString(const char *s);

static void DMA1_CH1_Config(void);
static void ADC1_Config(void);
static void ADC1_DMA_ConFig(void);
static void Start_ADC_Conversions(void);

static void delay_ms(uint32_t ms);

// ============================================================
int main(void)
{
    char msg[80];
    float volts[ADC_CHANNELS];

    UART1_Init();
    ADC1_Config();         // Configure ADC and perform calibration
    ADC1_DMA_ConFig();     // Configure ADC scan mode, DMA bit, and sequence
    DMA1_CH1_Config();     // Configure DMA (after ADC is ready)
	  Start_ADC_Conversions();
   

    UART1_SendString("=== ADC Multi-Channel DMA Demo ===\r\n");

    while (1)
    {
        for (uint8_t i = 0; i < ADC_CHANNELS; ++i)
            volts[i] = (adc_buf[i] * VREF) / ADC_RES;

        sprintf(msg, "CH1=%.2fV  CH2=%.2fV  CH3=%.2fV\r\n",
                volts[0], volts[1], volts[2]);
        UART1_SendString(msg);
        delay_ms(300);
    }
}

// ================= UART =================
static void UART1_Init(void)
{
    // Enable clocks for GPIOA and USART1
    RCC->APB2ENR |= (1U << 2) | (1U << 14);

    // Configure PA9 as TX (AF push-pull, 50 MHz), PA10 as RX (floating input)
    GPIOA->CRH &= ~((0xF << 4) | (0xF << 8)); // Clear PA9 and PA10 configs
    GPIOA->CRH |=  (0x0B << 4);               // PA9 TX: AF push-pull 50 MHz
    GPIOA->CRH |=  (0x04 << 8);               // PA10 RX: input floating

    // Set baud rate and enable USART1: UE, TE, RE
    USART1->BRR = BAUD_9600_BRR;
    USART1->CR1 = (1U << 13) | (1U << 3) | (1U << 2);
}

static void UART1_SendChar(char c)
{
    while (!(USART1->SR & (1U << 7)));   // Wait for TXE = 1
    USART1->DR = (uint8_t)c;
}

static void UART1_SendString(const char *s)
{
    while (*s)
    {
        if (*s == '\n')
            UART1_SendChar('\r');        // Convert LF ? CRLF
        UART1_SendChar(*s++);
    }
}

// ================= ADC =================
static void ADC1_Config(void)
{
    // Enable GPIOA and ADC1 clocks
    RCC->APB2ENR |= (1U << 2) | (1U << 9);

    // Set ADC prescaler: PCLK2 / 6 (bits 15:14 = 10)
    RCC->CFGR &= ~(0x3U << 14);
    RCC->CFGR |=  (0x2U << 14);

    // Configure PA0–PA2 as analog inputs (ADC_CH0–CH2)
    GPIOA->CRL &= ~((0xF << (ADC_CH1 * 4)) |
                    (0xF << (ADC_CH2 * 4)) |
                    (0xF << (ADC_CH3 * 4)));

    // Configure sample time for channels 0–2 to 239.5 cycles
    ADC1->SMPR2 &= ~((0x7U << (3 * ADC_CH1)) |
                     (0x7U << (3 * ADC_CH2)) |
                     (0x7U << (3 * ADC_CH3)));
    ADC1->SMPR2 |=  ((0x7U << (3 * ADC_CH1)) |
                     (0x7U << (3 * ADC_CH2)) |
                     (0x7U << (3 * ADC_CH3)));

    // Power on ADC
    ADC1->CR2 |= (1U << 0);              // ADON = 1 (wake up)
    delay_ms(1);

    // Reset calibration
    ADC1->CR2 |= (1U << 3);              // RSTCAL = 1
    while (ADC1->CR2 & (1U << 3));       // Wait until reset complete

    // Start calibration
    ADC1->CR2 |= (1U << 2);              // CAL = 1
    while (ADC1->CR2 & (1U << 2));       // Wait until calibration complete
}

static void ADC1_DMA_ConFig(void)
{
    // Enable scan mode
    ADC1->CR1 |= (1U << 8);                   // SCAN = 1

    // Enable continuous conversion and DMA request
    ADC1->CR2 |= (1U << 1);                   // CONT = 1
    ADC1->CR2 |= (1U << 8);                   // DMA = 1

    // Regular sequence length = N - 1
    ADC1->SQR1 &= ~(0xF << 20);
    ADC1->SQR1 |= (ADC_CHANNELS - 1U) << 20;  // L = N - 1

    // Define sequence: rank1 = CH0, rank2 = CH1, rank3 = CH2
    ADC1->SQR3 = (ADC_CH1 << 0) | (ADC_CH2 << 5) | (ADC_CH3 << 10);

    // Use software trigger (EXTSEL = 111 -> SWSTART)
    ADC1->CR2 &= ~(0x7U << 17);
    ADC1->CR2 |=  (0x7U << 17);               // EXTSEL = 111 (SWSTART)
    ADC1->CR2 &= ~(1U << 20);                 // EXTTRIG = 0 (disable external trigger)
}

// ================= DMA =================
static void DMA1_CH1_Config(void)
{
    // Enable DMA1 clock
    RCC->AHBENR |= (1U << 0);

    // Disable DMA channel before configuration
    DMA1_Channel1->CCR &= ~(1U << 0);         // EN = 0

    // Peripheral address (ADC1->DR)
    DMA1_Channel1->CPAR = (uint32_t)&ADC1->DR;

    // Memory address
    DMA1_Channel1->CMAR = (uint32_t)adc_buf;

    // Number of data to transfer
    DMA1_Channel1->CNDTR = ADC_CHANNELS;

    // Configure DMA:
    // CIRC = 1, MINC = 1, PSIZE = 16-bit, MSIZE = 16-bit, PL = very high
    DMA1_Channel1->CCR = 0;
    DMA1_Channel1->CCR |= (1U << 7);      // CIRC = 1 (circular mode)
    DMA1_Channel1->CCR |= (1U << 5);      // MINC = 1 (increment memory)
    DMA1_Channel1->CCR |= (1U << 8);      // PSIZE = 16-bit
    DMA1_Channel1->CCR |= (1U << 10);     // MSIZE = 16-bit
    DMA1_Channel1->CCR |= (0x3U << 12);   // PL = 11 (very high priority)
    DMA1_Channel1->CCR &= ~(1U << 4);     // DIR = 0 (peripheral -> memory)

    // Enable DMA channel
    DMA1_Channel1->CCR |= (1U << 0);      // EN = 1
}


static void Start_ADC_Conversions(void)
{
	// Start ADC conversions: ADON (2nd time) + SWSTART
    ADC1->CR2 |= (1U << 0);    // ADON = 1 (2nd time to start when using SWSTART in continuous mode)
    delay_ms(1);
    ADC1->CR2 |= (1U << 22);   // SWSTART = 1  start continuous conversions
}

// ================= SysTick delay =================
static void delay_ms(uint32_t ms)
{
    
    SysTick->LOAD = 72000 - 1U;
    SysTick->VAL  = 0U;
    SysTick->CTRL = (1U << 2) | (1U << 0); // CLKSOURCE = HCLK, ENABLE = 1

    while (ms--)
        while (!(SysTick->CTRL & (1U << 16))); // Wait for COUNTFLAG

    SysTick->CTRL = 0;
    SysTick->VAL  = 0;
}
