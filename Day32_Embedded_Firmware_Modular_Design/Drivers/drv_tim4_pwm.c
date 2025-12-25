#include "stm32f10x.h"
#include "drv_tim4_pwm.h"

void DRV_TIM4_PWM_Init(void)
{
    RCC->APB2ENR |= (1U << 3);
    RCC->APB1ENR |= (1U << 2);
    RCC->APB2ENR |= (1U << 0);

    GPIOB->CRH &= ~(0xFU << 4);
    GPIOB->CRH |=  (0x0B << 4);

    TIM4->PSC = 71;
    TIM4->ARR = 20000 - 1;
    TIM4->CCR4 = 1500;

    TIM4->CCMR2 |= (6U << 12) | (1U << 11);
    TIM4->CCER  |= (1U << 12);
    TIM4->CR1   = (1U << 7) | (1U << 0);
    TIM4->EGR  |= 1;
}

void DRV_TIM4_SetCCR(uint32_t value)
{
    TIM4->CCR4 = value;
}
