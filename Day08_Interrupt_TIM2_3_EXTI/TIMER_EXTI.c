#include "stm32f10x.h"

void GPIO_Config(void);
void TIM2_Config(uint16_t arr);
void EXTI_Config(void);
void TIM3_Config(uint16_t ms);

uint8_t speed_mode = 0;

int main(void)
{
    GPIO_Config();
    TIM2_Config(1000);     // LED blinks every 1s (default)
    EXTI_Config();

    while(1);
}

void GPIO_Config(void)
{
    RCC->APB2ENR |= (1<<2) | (1<<4); // Enable GPIOA, GPIOC clocks

    // PC13 as Output Push-Pull (LED)
    GPIOC->CRH &= ~(0xF << 20);
    GPIOC->CRH |=  (0x3 << 20);
	GPIOC->ODR |=  (1 << 13); 

    // PA1 as Input Pull-up (Button)
    GPIOA->CRL &= ~(0xF << 4);
    GPIOA->CRL |=  (0x8 << 4);   // Input with pull-up/down
    GPIOA->ODR |=  (1 << 1);     // Activate internal pull-up
}

void TIM2_Config(uint16_t arr)
{
    RCC->APB1ENR |= (1<<0);      // Enable TIM2 clock
	TIM2->CR1 = 0;
	TIM2->CNT = 0; 
	TIM2->SR &= ~(1<<0);         // Clear flag
    TIM2->PSC = 7199;            // 1 tick = 0.1ms (72MHz / 7200 = 10kHz)
    TIM2->ARR = 10U*arr;         // Period = arr (ms)
    TIM2->EGR |= (1<<0);         // Update registers immediately
    TIM2->DIER |= (1<<0);        // Enable update interrupt
    TIM2->CR1 |= (1<<0);         // Enable counter
    NVIC_EnableIRQ(TIM2_IRQn);
}

void TIM3_Config(uint16_t ms)
{
    RCC->APB1ENR |= (1<<1);      // Enable TIM3 clock
	TIM3->CR1 = 0;
	TIM3->CNT = 0;
	TIM3->SR &= ~(1<<0);         // Clear flag
    TIM3->PSC = 7199;            // 1 tick = 0.1ms
    TIM3->ARR = 10U*ms;            
    TIM3->EGR |= (1<<0);         // Update registers
    TIM3->DIER |= (1<<0);        // Enable update interrupt
    TIM3->CR1 |= (1<<0);         // Start counting
    NVIC_EnableIRQ(TIM3_IRQn);
}

void EXTI_Config(void)
{
    RCC->APB2ENR |= (1<<0);      // Enable AFIO clock
    AFIO->EXTICR[0] &= ~(0xF << 4);
    AFIO->EXTICR[0] |=  (0x0 << 4);  // Map PA1 -> EXTI1 line

    EXTI->IMR  |= (1<<1);        // Unmask EXTI1
    EXTI->FTSR |= (1<<1);        // Trigger on falling edge
    EXTI->RTSR &= ~(1<<1);       // Disable rising edge
    NVIC_EnableIRQ(EXTI1_IRQn);  // Enable EXTI1 interrupt
}

void TIM2_IRQHandler(void)
{
    if (TIM2->SR & (1<<0))
	{
        GPIOC->ODR ^= (1<<13);   // Toggle LED
        TIM2->SR &= ~(1<<0);     // Clear interrupt flag
    }
}

void EXTI1_IRQHandler(void)
{
    if (EXTI->PR & (1<<1))
	{
        // Disable EXTI1
        EXTI->IMR &= ~(1<<1);

        // Toggle blinking speed
        speed_mode = !speed_mode;
        if (speed_mode)
            TIM2->ARR = 2000;     // Fast (200ms)
        else
            TIM2->ARR = 10000;    // Slow (1s)
				
			TIM2->CNT = 0;
        TIM2->EGR |= (1<<0);     // Force update

        // Start 50ms debounce timer
        TIM3_Config(50);

        // Clear EXTI1 pending flag
        EXTI->PR = (1<<1);
    }
}

void TIM3_IRQHandler(void)
{
    if (TIM3->SR & (1<<0))
	{
        // After 50ms, re-enable EXTI1
        EXTI->IMR |= (1<<1);
        TIM3->SR  &= ~(1<<0);     // Clear flag
        TIM3->CR1 &= ~(1<<0);    // Stop Timer3
    }
}
