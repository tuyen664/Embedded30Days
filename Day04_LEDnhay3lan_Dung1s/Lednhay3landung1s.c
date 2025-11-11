#include "stm32f10x.h"


void Delay_Ms(uint32_t u32Delay)
{
	SysTick->LOAD = 72U*1000U-1U; // 72MHz(~1s)-> 72000 (~1ms)
	SysTick->VAL = 0U;    // clear curent counter value
	SysTick->CTRL = 5U;  // enable sysTick and select HCLK = 72MHz 
	while(u32Delay)
	{
		while (!(SysTick->CTRL & (1U << 16U)))
		{
		}
		--u32Delay;
	}
	SysTick->CTRL = 0U;  // disable SysTick
}

void ConFigPinC13(void)
{
	RCC->APB2ENR |= 0x10; // enable GPIOC
	GPIOC->CRH &= 0xFF0FFFFF; // clear PC13
	GPIOC->CRH |= 0x00300000; // PC13 - 0011 as output push-pull /50MHz
}

int main (void)
{
	ConFigPinC13();
	while (1)
	{
    uint32_t count= 3U;
		
		while (count)
		{
			GPIOC->BSRR = (1 << (13 + 16)); // reset PC13 -> LED on
			Delay_Ms(200U);
			GPIOC->ODR ^= (1 << 13); // Toggle output
			Delay_Ms(200U);
			count--; // Decrement blink couter
		}
	
		Delay_Ms(1000U); // wait 1s before next sequence
	
	}
	
	/*while (1)
	{
   for (uint32_t i = 0; i < 3; ++i)
        {
            
            GPIOC->BSRR = (1 << (13 + 16)); // LED ON
            Delay_Ms(200U);

            
            GPIOC->BSRR = (1 << 13);   // LED OFF   
            Delay_Ms(200U);
        }

        
        Delay_Ms(1000U);
    }
	*/
	
	
}