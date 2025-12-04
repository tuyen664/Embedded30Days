#include "stm32f10x.h"

void GPIO_Config(void);
void TIM2_Config(void);

int main(void) 
{
    GPIO_Config();
    TIM2_Config();

    while (1) 
	{
        
    }
}

void GPIO_Config(void)
{
    RCC->APB2ENR |= (1 << 4);       // Enable clock GPIOC
    GPIOC->CRH &= ~(0xF << 20);     // PC13 Output
    GPIOC->CRH |=  (0x2 << 20);
}


void TIM2_Config(void)
{
	
	 RCC->APB1ENR |= (1<<0); // bat clock cho TIM2
	 TIM2->CR1 = 0; // Dua TIM2 ve trang thai mac dinh
     TIM2->CNT = 0; // Reset bo dem ve 0
	
	 TIM2->PSC = 7199 ; // 72MHz / 7200 = 10KHz -> 0.1ms/tick
	 TIM2->ARR = 5000-1 ; // 5000/10KHz = 0.5s
	
	 TIM2->EGR |= (1 << 0);     // <--- NAP PSC & ARR NGAY
	
	 TIM2->DIER |= (1<<0); // cho phep ngat
	 TIM2->CR1 |= (1<<0);  // bat Counter Enable (CEN)
	 NVIC_EnableIRQ(TIM2_IRQn);
}


void TIM2_IRQHandler(void) 
{    // Check update flag
    if (TIM2->SR & (1 << 0)) 
	{    
        TIM2->SR &= ~(1 << 0);      // Clear flag
        GPIOC-> ODR ^= (1 << 13);     // Toggle LED
    }
}
