#include "stm32f10x.h"

void GPIO_Config(void);
void EXTI_Config(void);

int main(void) 
{
    GPIO_Config();
    EXTI_Config();

    while (1) 
		{
          
        }
}


/* Cau hinh GPIO */

void GPIO_Config(void)
{
    // Bat clock cho GPIOA, GPIOC va AFIO
    RCC->APB2ENR |= (1 << 2) | (1 << 4) | (1 << 0);

    // PC13: Output push-pull (LED)
    GPIOC->CRH &= ~(0xF << 20);   // clear bits Mode - CNF13
    GPIOC->CRH |=  (0x2 << 20);   // Output push-pull, max 2MHz

    // PA1: Input pull-up (nut nhan)
    GPIOA->CRL &= ~(0xF << 4);    // clear bits cho PA1
    GPIOA->CRL |=  (0x8 << 4);    // Input pull-up / pull down
    GPIOA->ODR |=  (1 << 1);      // Kéo PA1 Input lên (pull-up)
}


/* Cau hinh EXTI cho PA1 */

void EXTI_Config(void) 
{
    // Ket noi EXTI1 voi PA1
    AFIO->EXTICR[0] &= ~(0xF << 4);  // clear mapping EXTI1
    AFIO->EXTICR[0] |=  (0x0 << 4);  /* chon port A (0b0000) - line 1 (bit 4-7) */

    // Cho phep ngat o line 1
    EXTI->IMR  |= (1 << 1);   // Unmask line 1 -> cho phep line 1 phat ngat
    EXTI->FTSR |= (1 << 1);   // Ngat canh xuong 

    // Bat ngat trong NVIC
    NVIC_EnableIRQ(EXTI1_IRQn);
}


/* Ham xu ly ngat */

void EXTI1_IRQHandler(void) 
{
	// Kiem tra co ngat line 1
    if (EXTI->PR & (1 << 1))
	{    
        GPIOC->ODR ^= (1 << 13);   // Ðao LED
        EXTI->PR = (1 << 1);      // Xóa co ngat
    }
}
