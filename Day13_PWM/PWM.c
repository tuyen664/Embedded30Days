#include "stm32f10x.h"
#include <stdint.h>

#define PWM_MAX 1000     // ARR value (PWM resolution)
#define STEP    1 

static void TIM2_Interrupt_Init(void);
static void TIM3_PWM_Init(void);



int main(void)
{
    TIM3_PWM_Init();
    TIM2_Interrupt_Init();

    while(1)
    {
       //
    }
}

void TIM2_IRQHandler(void);

volatile uint16_t duty = 0;
volatile int8_t direction = 1;


static void TIM2_Interrupt_Init(void)
{
    RCC->APB1ENR |= (1 << 0);  // Enable TIM2 clock

    TIM2->PSC = 7199;          // 72MHz / 7200 = 10 kHz
    TIM2->ARR = 10 -1 ;             // interrupt ~ 1ms
	  TIM2->EGR |= 1;
    TIM2->DIER |= (1 << 0);     // cho phep ngat
    TIM2->CR1  |= (1 << 0);     // enable TIM2

    NVIC_EnableIRQ(TIM2_IRQn);  // Enable TIM2 interrupt in NVIC
}

void TIM2_IRQHandler(void)
{
    if(TIM2->SR & (1 << 0))  // Check update interrupt flag
    {
        TIM2->SR &= ~(1 << 0);  // Clear interrupt flag

        duty += direction * STEP;
        if(duty >= PWM_MAX) { duty = PWM_MAX; direction = -1; }
        else if(duty == 0)  { direction = 1; }

        TIM3->CCR1 = duty;  // Update PWM
    }
}

static void TIM3_PWM_Init(void)
{
    // Enable GPIOA & TIM3 clock
    RCC->APB2ENR |= (1 << 2);  // GPIOA
    RCC->APB1ENR |= (1 << 1);  // TIM3

    // Configure PA6 as AF Push-Pull @50MHz
    GPIOA->CRL &= ~(0xF << 24);
    GPIOA->CRL |=  (0xB << 24);

    // PWM frequency = 72MHz / (PSC+1) / (ARR+1)
    TIM3->PSC = 72-1;        
    TIM3->ARR = PWM_MAX -1U ;       
    TIM3->CCR1 = 0 ;    // 0% H     

    // Clear OC1M & OC1PE bits
    TIM3->CCMR1 &= ~((7 << 4) | (1 << 3)); // clear bit 3 , bit4-7
    TIM3->CCMR1 |= (6 << 4) | (1 << 3);  // PWM mode1 1 + preload enable

    TIM3->CCER |= (1 << 0);   // enable channel 1 
    TIM3->CR1  |= (1 << 7) | (1 << 0); // ARPE + CEN
	
	  TIM3->EGR |= (1 << 0);   // Force Update Event
}


