#include "stm32f10x.h"

void Delay_Ms(uint32_t u32Delay);

void GPIO_ConfigPinC13(void)
{
	RCC->APB2ENR |= 0x10 ;
	GPIOC->CRH &= 0xFF0FFFFF ;
	GPIOC->CRH |= 0x00300000;

}

int main(void)
{
    GPIO_ConfigPinC13(); 

    while(1)
    {
      
        GPIOC->ODR ^= (1 << 13);
        Delay_Ms(1000U);
    }
}

void Delay_Ms(uint32_t u32Delay)
{
	while(u32Delay)
	{
		SysTick->LOAD = 72U*1000U-1U;
	  SysTick->VAL=0U;
	  SysTick->CTRL=5U;
	  while(!(SysTick->CTRL&(1U<<16U)))
    {
	  }
	   --u32Delay;
  }
}