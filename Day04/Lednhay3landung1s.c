#include "stm32f10x.h"


void Delay_Ms(uint32_t u32Delay)
{
	SysTick->LOAD = 72U*1000U-1U;
	SysTick->VAL = 0U;
	SysTick->CTRL = 5U;
	while(u32Delay)
	{
		while (!(SysTick->CTRL & (1U << 16U)))
		{
		}
		--u32Delay;
	}
	SysTick->CTRL = 0U;
}

void ConFigPinC13(void)
{
	RCC->APB2ENR |= 0x10;
	GPIOC->CRH &= 0xFF0FFFFF;
	GPIOC->CRH |= 0x00300000;
}

int main (void)
{
	ConFigPinC13();
	while (1)
	{
    uint32_t count= 3U;
		
		while (count)
		{
			GPIOC->BSRR = (1 << (13 + 16));
			Delay_Ms(200U);
			GPIOC->ODR ^= (1 << 13);
			Delay_Ms(200U);
			count--;
		}
	
		Delay_Ms(1000U);
	
	}
	
	/*while (1)
	{
   for (uint32_t i = 0; i < 3; ++i)
        {
            
            GPIOC->BSRR = (1 << (13 + 16));
            Delay_Ms(200U);

            
            GPIOC->BSRR = (1 << 13);      
            Delay_Ms(200U);
        }

        
        Delay_Ms(1000U);
    }
	*/
	
	
}