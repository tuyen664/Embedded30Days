#include "stm32f10x.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define BAUD_9600_BRR   0x1D4C    // 9600 baud
#define SYSCLK_HZ       72000000UL


static void System_Init(void);
static void UART1_Init(void);
static void UART1_SendString(const char *s);

static void TIM4_PWM_Init(void);
static void TIM3_PWM_Init(void);

static void SysTick_Init(void);
static void delay_ms(uint32_t ms);

static void ADC1_Config(void);
static uint16_t readADC_Avg(uint8_t channel , uint8_t samples);

static float PID_Update(float set, float real);
static uint32_t millis (void);

float kp = 2.0f;
float ki = 0.1f;
float kd = 0.05f;
float prev_error = 0, integral = 0;


int main(void)
{
	System_Init();
	
	float setTemp = 35.0f;  // Nhiet Do Muc Tieu
    char buffer[100];
	
	 while (1)
    {
            uint16_t adcTemp = readADC_Avg(0 , 5 ); // LM35
			uint16_t adcLdr = readADC_Avg( 1 , 5 );  // LDR
			
			float tempC = (adcTemp * 3.3f / 4095.0f) / 0.01f;   // LM35: 10mV/°C
			
			uint16_t dutyFan = (uint16_t)PID_Update(setTemp, tempC);
			uint16_t dutyLed = 100 - (uint16_t) (adcLdr / 4095.0f * 100);
			
			TIM4->CCR4 = (dutyFan * (TIM4->ARR + 1)) / 100;
			TIM3->CCR1 = (dutyLed * (TIM3->ARR + 1)) / 100;
			
			sprintf(buffer, "Temp=%.1fC,Fan=%d%%,LED=%d%%\r\n",
            tempC, dutyFan, dutyLed);
			
			UART1_SendString(buffer);
		
      delay_ms(500); // dong bo voi chu ki pid
    }
	
    
}

float PID_Update(float set, float real)
{
	static uint32_t last = 0; // goi static thi chi khoi tao 1 lan duy nhat
    uint32_t now = millis();
    float dt = (now - last) / 1000.0f;
    last = now;
	

    float error = set - real;        // Sai so

    // Vung chet (deadband): bo qua sai so nho de tranh dao dong
    if (fabs(error) < 0.2f) error = 0;

    // Thanh phan tich phan (I)
    integral += error * dt;
    if (integral > 100) integral = 100;
    if (integral < -100) integral = -100;

    // Thanh phan vi phân (D)
    float derivative = (error - prev_error) / dt;
    prev_error = error;

    float output = kp * error + ki * integral + kd * derivative;

    if (output > 100) output = 100;
    if (output < 0) output = 0;

    // Chong bao hoa tich phan (Anti-windup)
    if (output == 0 || output == 100)
        integral -= error * dt;

    return output;   // Tra ve duty cycle (%)
}


static void System_Init(void)
{
    SysTick_Init();
    UART1_Init();
    TIM4_PWM_Init();   // PWM FAN (PB9)
	TIM3_PWM_Init();   // PWM LED (PB6)
    ADC1_Config();     // Cau hinh ADC1 doc kênh 0 & 1
}

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

static void TIM4_PWM_Init(void)
{
    RCC->APB2ENR |= (1U << 3); // GPIOB clock
    RCC->APB1ENR |= (1U << 2); // TIM4 clock
    RCC->APB2ENR |= (1U << 0); // AFIO clock

    // PB9: Alternate Function Push-Pull output
    GPIOB->CRH &= ~(0xFU << 4);
    GPIOB->CRH |=  (0x0B << 4);

    TIM4->PSC = 71;              // 1MHz timer clock (72MHz / (71+1))
    TIM4->ARR = 1000 - 1;       // 1khz PWM -> led muot
    TIM4->CCR4 = 0;           // duty = 0
	
    TIM4->CCMR2 |= (6U << 12) | (1U << 11); 
	// OC4M = 110 (14-12) -> PWM MODE 1  , OC4PE = 1 (bit 11) -> enable Preload
	// preload -Gia tri moi ghi vao CCR4 chi co hieu luc khi co su kien update
    TIM4->CCER  |= (1U << 12);    // Enable output on CH4
	// TIM4->CCER |= (1U << 13); // Active low output
    TIM4->CR1   = (1U << 7) | (1U << 0); // ARPE + Counter enable
	// ARPE  : giup pwm on dinh khi thay doi ARR

    TIM4->EGR  |= 1;             // Update event
}

static void TIM3_PWM_Init(void)
{
    RCC->APB2ENR |= (1U << 3); // GPIOB clock
    RCC->APB1ENR |= (1U << 1); // TIM3 clock
    RCC->APB2ENR |= (1U << 0); // AFIO clock

    // PB6: Alternate Function Push-Pull output
    GPIOB->CRH &= ~(0xFU << 24); //  PB6 Clear
    GPIOB->CRH |=  (0x0B << 24); // PB6 AF Push-Pull

    TIM3->PSC = 71;              // 1MHz timer clock (72MHz / (71+1))
    TIM3->ARR = 1000 - 1;        // 1khz PWM -> led muot
    TIM3->CCR1 = 0;              // duty = 0
	
    TIM3->CCMR1 |= (6U << 4) | (1U << 3); 
	// OC4M = 110 (6-4) -> PWM MODE 1  , OC4PE = 1 (bit 3) -> enable Preload
	// preload -Gia tri moi ghi vao CCR4 chi co hieu luc khi co su kien update
    TIM3->CCER  |= (1U << 0);    // Enable output on CH1
	// TIM3->CCER |= (1U << 1); // Active low output
    TIM3->CR1   = (1U << 7) | (1U << 0); // ARPE + Counter enable
	// ARPE  : giup pwm on dinh khi thay doi ARR

    TIM3->EGR  |= 1;             // Update event
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
			  delay_ms(2);
    }
    return (uint16_t)(sum / samples);
}




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

static uint32_t millis (void)
{ 
	return tick_ms ;
}
