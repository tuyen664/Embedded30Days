#include "stm32f10x.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ================== CONFIG CONSTANTS ==================
#define SYSCLK_HZ       72000000UL
#define UART_BAUDRATE   9600U

#define SERVO_MIN_US    600U      // Minimum pulse width for servo (˜0°)
#define SERVO_MAX_US    2400U     // Maximum pulse width for servo (˜180°)
#define PWM_FREQ_HZ     50U       // 50Hz = 20ms period (standard servo)

#define ADC_CHANNELS    3         // Using 3 ADC channels: PA0, PA1, PA2
#define VREF            3.3f
#define ADC_RES         4095.0f
#define BAUD_9600_BRR   0x1D4C    // Pre-calculated for 72MHz -> 9600 baud

#define MANUAL_TIMEOUT_MS  5000   // Return to auto mode after 5s idle

// ================== GLOBAL VARIABLES ==================
static volatile uint16_t adc_buf[ADC_CHANNELS];
static uint32_t last_manual_time = 0;
static uint8_t manual_mode = 0;


// ================== FUNCTION PROTOTYPES ==================
static void System_Init(void);
static void UART1_Init(void);
static void UART1_SendString(const char *s);
static char UART1_ReceiveChar(void);
static void TIM4_PWM_Init(void);
static void ADC1_StartConversion(void);
static void Servo_SetAngle(uint8_t angle);
static void SysTick_Init(void);
static void delay_ms(uint32_t ms);
static uint32_t millis(void);

static void ADC1_Config(void);
static void ADC1_DMA_Config(void);
static void DMA1_CH1_Config(void);

// ================== MAIN ==================
int main(void)
{
    char msg[64];
    char rx_buf[16];
    uint8_t i = 0;
    float volts[ADC_CHANNELS];
    uint8_t servo_angle = 90;
    uint32_t last_auto_update = 0;

    System_Init();
    UART1_SendString("\r\n=== Mini Robot: ADC + PWM Servo + UART ===\r\n");
	
    Servo_SetAngle(servo_angle);

    // Start continuous ADC conversion
	  ADC1_StartConversion();
	
    while (1)
    {
			
        // ----- UART Command (Manual Servo Control) -----
        
       if (USART1->SR & (1U << 5)) // RXNE flag = 1 (data received)
        {
            char c = UART1_ReceiveChar();
					
					 if (i == 0) // nhap ki tu dau tien la auto stop ngay
            {
              manual_mode = 1;
              last_manual_time = millis(); // luu lai thoi diem bat dau manual , qua 5s chuyen auto
            }

            if (c == '\r' || c == '\n')
            {
                rx_buf[i] = '\0';
                int angle = atoi(rx_buf);
                if (angle < 0) angle = 0;
                if (angle > 180) angle = 180;
				
                Servo_SetAngle((uint8_t)angle);
							
							  sprintf(msg, " \r\n Set servo %d deg \r\n",angle);
                UART1_SendString(msg);
                
                i = 0;

                manual_mode = 1;
                last_manual_time = millis();
            }
            else if (c >= '0' && c <= '9' && i < sizeof(rx_buf) - 1)
            {
                rx_buf[i++] = c;
            }
           /* else 
            {
                i = 0; // reset buffer on invalid char
            } */
						else if (c < '0' || c > '9') 
						{
							// invalid char -> ignore, do not reset
						}
        }

        // ----- Auto Servo Control (based on ADC values) -----
        if (manual_mode == 0 || (millis() - last_manual_time > MANUAL_TIMEOUT_MS))
        {
            manual_mode = 0; // back to auto mode
            if (millis() - last_auto_update > 500)
            {
                last_auto_update = millis();
							
							 // ----- Read ADC values -----
							 for (uint8_t ch = 0; ch < ADC_CHANNELS; ch++)
                volts[ch] = (adc_buf[ch] * VREF) / ADC_RES;

                sprintf(msg, "CH1=%.2fV  CH2=%.2fV  CH3=%.2fV\r\n",
                volts[0], volts[1], volts[2]);
                UART1_SendString(msg);

                float left = volts[0];
             
                float right = volts[2];

                // Simple decision logic
                if (left > right + 0.3f)
                    servo_angle = 180;   // turn left
                else if (right > left + 0.3f)
                    servo_angle = 0;    // turn right
                else
                    servo_angle = 90;    // center

                Servo_SetAngle(servo_angle);
                
            }
        }

        delay_ms(10);
    }
}

// ================== SYSTEM INITIALIZATION ==================
static void System_Init(void)
{
    SysTick_Init();
    UART1_Init();
    TIM4_PWM_Init();
    ADC1_Config();
    ADC1_DMA_Config();
    DMA1_CH1_Config();
}

// ================== UART1 ==================
static void UART1_Init(void)
{
    RCC->APB2ENR |= (1U << 2) | (1U << 14); // Enable GPIOA + USART1 clocks

    // PA9 = TX (AF Push-Pull), PA10 = RX (Input Floating)
    GPIOA->CRH &= ~((0xFU << 4) | (0xFU << 8));
    GPIOA->CRH |=  (0x0B << 4) | (0x04 << 8);

    USART1->BRR = BAUD_9600_BRR;
    USART1->CR1 = (1U << 13) | (1U << 3) | (1U << 2); // UE, TE, RE
}

static void UART1_SendString(const char *s)
{
    while (*s)
    {
        while (!(USART1->SR & (1U << 7))); // Wait TXE = 1
        USART1->DR = *s++;
    }
}

static char UART1_ReceiveChar(void)
{
    while (!(USART1->SR & (1U << 5))); // Wait RXNE = 1
    return (char)(USART1->DR & 0xFF);
} 


// ================== SERVO (PWM - TIM4_CH4 PB9) ==================
static void TIM4_PWM_Init(void)
{
    RCC->APB2ENR |= (1U << 3); // GPIOB clock
    RCC->APB1ENR |= (1U << 2); // TIM4 clock
    RCC->APB2ENR |= (1U << 0); // AFIO clock

    // PB9: Alternate Function Push-Pull output
    GPIOB->CRH &= ~(0xFU << 4);
    GPIOB->CRH |=  (0x0B << 4);

    TIM4->PSC = 71;              // 1MHz timer clock (72MHz / (71+1))
    TIM4->ARR = 20000 - 1;       // 50hz->20ms period
    TIM4->CCR4 = 1500;           // Default 1.5ms -> Servo-90 deg
	
    TIM4->CCMR2 |= (6U << 12) | (1U << 11); 
	// OC4M = 110 (14-12) -> PWM MODE 1  , OC4PE = 1 (bit 11) -> enable Preload
	// preload -Gia tri moi ghi vao CCR4 chi co hieu luc khi co su kien update
    TIM4->CCER  |= (1U << 12);    // Enable output on CH4
	// TIM4->CCER |= (1U << 13); // Active low output
    TIM4->CR1   = (1U << 7) | (1U << 0); // ARPE + Counter enable
	// ARPE  : giup pwm on dinh khi thay doi ARR

    TIM4->EGR  |= 1;             // Update event
}

static void Servo_SetAngle(uint8_t angle)
{
    if (angle > 180) angle = 180;
    if (angle < 0) angle = 0 ;
    uint32_t pulse = SERVO_MIN_US +
                     ((SERVO_MAX_US - SERVO_MIN_US) * angle) / 180U;
    TIM4->CCR4 = pulse;
}

// ================== ADC + DMA ==================
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
}

static void ADC1_DMA_Config(void)
{
    ADC1->CR1 |= (1U << 8);                // Scan mode
    ADC1->CR2 |= (1U << 1) | (1U << 8);    // Continuous mode + DMA enable
    ADC1->SQR1 = (ADC_CHANNELS - 1) << 20; // 3 conversions
    ADC1->SQR3 = (0U << 0) | (1U << 5) | (2U << 10); // CH0, CH1, CH2
    ADC1->CR2 &= ~(0x7U << 17);
    ADC1->CR2 |=  (0x7U << 17);            // Software trigger
    ADC1->CR2 &= ~(1U << 20);              // Disable external trigger
}

static void DMA1_CH1_Config(void)
{
    RCC->AHBENR |= (1U << 0);              // DMA1 clock
    DMA1_Channel1->CCR &= ~(1U << 0);      // Disable channel
	
  // noi DMA doc du lieu
    DMA1_Channel1->CPAR = (uint32_t)&ADC1->DR;
	
	// Noi DMA ghi du lieu da doc vao
    DMA1_Channel1->CMAR = (uint32_t)adc_buf;
	
    DMA1_Channel1->CNDTR = ADC_CHANNELS;

    // Circular mode, memory increment, 16-bit data
    DMA1_Channel1->CCR = 0;
    DMA1_Channel1->CCR |= (1U << 7);    // Circular mode
    DMA1_Channel1->CCR |= (1U << 5);    // Memory increment
    DMA1_Channel1->CCR |= (1U << 8);    // Peripheral size 16-bit
    DMA1_Channel1->CCR |= (1U << 10);   // Memory size 16-bit
    DMA1_Channel1->CCR |= (0x3U << 12); // Priority: Very high

    DMA1_Channel1->CCR &= ~(1U << 4); 
	// DIR = 0 -> Peripheral -> Memory
	// DIR = 1 -> Memory -> Peripheral
	
    DMA1_Channel1->CCR |= (1U << 0);      // Enable DMA channel
}

static void ADC1_StartConversion(void)
{
  ADC1->CR2 |= (1U << 0);      // ADON lan 2 
    delay_ms(1);
  ADC1->CR2 |= (1U << 22);     // Start conversion (SWSTART)	
}

// ================== DELAY (SYSTICK) ==================
volatile uint32_t tick_ms = 0;

static void SysTick_Init(void)
{
    SysTick->LOAD = (SYSCLK_HZ / 1000U) - 1U;
    SysTick->VAL  = 0;
    SysTick->CTRL = 7U; // Enable, TickInt, AHB clock
}

void SysTick_Handler(void)
{
    tick_ms++;
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = tick_ms;
    while ((tick_ms - start) < ms);
}

static uint32_t millis(void)
{
    return tick_ms;
}

