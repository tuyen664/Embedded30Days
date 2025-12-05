#include "stm32f10x.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*================== CONFIG CONSTANTS ==================*/
#define SYSCLK_HZ       72000000UL // Unsigned Long
#define UART_BAUDRATE   9600U

#define SERVO_MIN_US    600U    // 600µs = 0.6 ms pulse - 0°
#define SERVO_MAX_US    2400U   // 2.4 ms pulse -180°
#define PWM_FREQ_HZ     50U     // 50 Hz = 20 ms period

/* ================== FUNCTION PROTOTYPES ==================*/
static void System_Init(void);
static void UART1_Init(void);
static void UART1_SendString(const char *s);
static char UART1_ReceiveChar(void);
static void TIM4_PWM_Init(void);
static void Servo_SetAngle(uint8_t angle);
static void SysTick_Init(void);
static void delay_ms(uint32_t ms);

/* ================== MAIN ==================*/
int main(void)
{
    char buffer[32];
	char outbuf[32];
    
    System_Init();
    UART1_SendString("\r\n=== Servo Control Ready ===\r\n");
    UART1_SendString("Send angle (0-180) \r\n");

   while (1)
{
    uint8_t i = 0;

    while (1)
    {
        char c = UART1_ReceiveChar();

        if (c == '\r' || c == '\n')
            break;

        if ((c >= '0' && c <= '9') && i < sizeof(buffer) - 1)
            buffer[i++] = c;
        else
        {
            UART1_SendString("Invalid input. Enter 0-180.\r\n");
            i = 0; // reset
            break;
        }
    }

    if (i == 0) // kiem tra xem có du lieu khong
        continue;

    buffer[i] = '\0';
    int angle = atoi(buffer);
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    Servo_SetAngle((uint8_t)angle);
    delay_ms(300);

    snprintf(outbuf ,sizeof(outbuf), "Angle: %d deg\r\n", angle);
    UART1_SendString(outbuf);
}

}

/* ================== SYSTEM INITIALIZATION ================== */
static void System_Init(void)
{
    SysTick_Init();
    UART1_Init();
    TIM4_PWM_Init();
}

/* ================== UART1 IMPLEMENTATION ================== */
static void UART1_Init(void)
{
    // Enable clocks for GPIOA and USART1
    RCC->APB2ENR |= (1U << 2) | (1U << 14);

    // PA9: TX (AF Push-Pull), PA10: RX (Input floating)
    GPIOA->CRH &= ~((0xFU << 4) | (0xFU << 8));
    GPIOA->CRH |= (0x0B << 4) | (0x04 << 8);

    // Baud rate = SYSCLK / USARTDIV
    USART1->BRR = 0x1D4C; // 9600 bps @ 72 MHz
    USART1->CR1 = (1U << 13) | (1U << 3) | (1U << 2); // UE, TE, RE
}

static void UART1_SendString(const char *s)
{
    while (*s)
    {
       while (!(USART1->SR & (1U << 7))); // Wait until TXE = 1
       USART1->DR = *s++;
    }
}

static char UART1_ReceiveChar(void)
{
    while (!(USART1->SR & (1U << 5))); // Wait until RXNE = 1
    return (char)(USART1->DR & 0xFF);
}

/*================== SERVO CONTROL USING TIM4_CH4 (PB9) ==================*/
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

    TIM4->PSC = 72-1; // 1 tick = 1µs
    TIM4->ARR = 20000-1; // 50Hz

    TIM4->CCR4 = 1500; // Default 1.5ms - 90°
    TIM4->CCMR2 |= (6U << 12) | (1U << 11);  // PWM mode 1 + preload enable
    TIM4->CCER  = (1U << 12);              // Enable output on CH4
    TIM4->CR1   = (1U << 7) | (1U << 0);   // ARPE + Counter enable
		
    TIM4->EGR |= 1; // Generate update event
	delay_ms(20);
}

static void Servo_SetAngle(uint8_t angle)
{
    if (angle > 180) angle = 180;

    uint32_t pulse = SERVO_MIN_US +
                     ((SERVO_MAX_US - SERVO_MIN_US) * angle) / 180U;
    TIM4->CCR4 = pulse;
	TIM4->EGR |= 1;
	delay_ms (10);
}

// ================== DELAY USING SYSTICK ==================
static void SysTick_Init(void)
{
    SysTick->LOAD = (SYSCLK_HZ / 1000U) - 1U; // 1 ms tick
    SysTick->VAL = 0;
    SysTick->CTRL = 5U; // Enable SysTick, HCLK = 72MHz
}

static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        while (!(SysTick->CTRL & (1U << 16))); // Wait for COUNTFLAG
    }
}
