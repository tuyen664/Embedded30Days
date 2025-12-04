#include "stm32f10x.h"

void Delay_Ms(uint32_t u32Delay);

void GPIO_ConfigPinC13(void)
{
	RCC->APB2ENR |= 0x10 ; // enable GPIOC (Bit 4 in APB2ENR)
	GPIOC->CRH &= 0xFF0FFFFF ; // Clear PC13
	GPIOC->CRH |= 0x00300000; // 0011 -> push-pull / output mode , maxspeed 50 Mhz
	GPIOC->BSRR |= (1<<13);

}

int main(void)
{
    GPIO_ConfigPinC13(); 

    while(1)
    {
      
        GPIOC->ODR ^= (1 << 13); // Toggle PC13 
        Delay_Ms(1000U);
    }
}

void Delay_Ms(uint32_t u32Delay)
{
	while(u32Delay)
	{
	  SysTick->LOAD = 72U*1000U-1U; // 71999 -> 0 -> 72000 xung clock -> 1ms
	  SysTick->VAL=0U;  
	  SysTick->CTRL=5U;
       /* 
        bit 0 : Enable SysTick
		bit 1 : TICKINT : bat ngat
		bit 2 : CLKSOURCE : 1 -> (CLOCK CPU)HCKL = 72MHz , 0 = HCKL/8
		bit 16: COUNTFLAG : Co bao dem xong (auto clear khi doc)
		*/		

	  while(!(SysTick->CTRL&(1U<<16U)))
      {
	  }
	   --u32Delay;
   }
}
