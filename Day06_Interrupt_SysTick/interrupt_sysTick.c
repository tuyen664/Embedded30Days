#include "stm32f10x.h"

volatile uint8_t buttonPressed = 0;
volatile uint32_t debounceTime = 0;

void GPIO_Config(void);
void EXTI_Config(void);
void SysTick_Config(void);

int main(void) 
{
    GPIO_Config();
    EXTI_Config();
    SysTick_Config_Custom();

    while (1)
	{
        
    }
}


void GPIO_Config(void) 
{
    // Bat clock cho GPIOA, GPIOC, AFIO
    RCC->APB2ENR |= (1 << 2) | (1 << 4) | (1 << 0);

    // PC13: Output push-pull, max 2MHz (LED)
    GPIOC->CRH &= ~(0xF << 20);  // Xoa cau hinh cho PC13
    GPIOC->CRH |=  (0x2 << 20);  // Output push-pull, 2 MHz

    // PA1: Input pull-up 
    GPIOA->CRL &= ~(0xF << 4);   // Xóa cau hình cho PA1
    GPIOA->CRL |=  (0x8 << 4);   // Input pull-up/down
    GPIOA->ODR |=  (1 << 1);     // Kéo lên pull-up
}


void EXTI_Config(void)
{
    // Gán EXTI1 PA1
    AFIO->EXTICR[0] &= ~(0xF << 4);  // Xóa bit map EXTI1
    AFIO->EXTICR[0] |=  (0x0 << 4);  // Chon port A cho line 1

    // Cho phép ngat EXTI1, kích hoat canh xuong
    EXTI->IMR  |= (1 << 1);   // Cho phép line 1
    EXTI->FTSR |= (1 << 1);   // Ngat khi có canh xuong

    // Bat ngat EXTI1 trong NVIC
    NVIC_EnableIRQ(EXTI1_IRQn);
}


void SysTick_Config(void) 
{
    SysTick->LOAD = 72000 - 1;  // 1ms neu HCLK = 72MHz
    SysTick->VAL  = 0;
    SysTick->CTRL = 7;          // Enable sysTick + interrupt + CPU clock 72MHz

}


void EXTI1_IRQHandler(void) 
{   // Kiem tra co ngat line 1
    if (EXTI->PR & (1 << 1)) 
	{ 
        EXTI->PR = (1 << 1);      // Xóa co ngat
        buttonPressed  = 1;         // Ðanh dau có nhan nut
        debounceTime = 50;         // Chong doi 50ms
        NVIC_DisableIRQ(EXTI1_IRQn); // Tam tat ngat
    }
}


void SysTick_Handler(void)
{
	if(debounceTime >0 )
	{
		-- debounceTime ;
		if(debounceTime == 0 && buttonPressed )
		{
			if(!(GPIOA-> IDR & ( 1<<1) ))
			{
				GPIOC->ODR ^= (1 << 13);
			}
			buttonPressed = 0;
			NVIC_EnableIRQ(EXTI1_IRQn);
		}
	}
		
}





