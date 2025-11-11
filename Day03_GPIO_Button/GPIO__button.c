#include "stm32f10x.h"

uint32_t  u32Count = 0u;


void Delay_Ms(uint32_t u32Delay)
{
	while(u32Delay)
	{
		SysTick->LOAD = 72U*1000U - 1U; // 72Mhz(~1s) -> 72000 (~1ms)
		SysTick->VAL = 0U;    // clear curent sysTick value
		SysTick->CTRL =5U;    // Enable SysTick and select HCLK = 72MHz
		while( !(SysTick->CTRL&(1U<<16U))) 
		{ 
			/* */
		}
		--u32Delay;
	}

}



void GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOA, ENABLE);

    // PC13 as Output Push-Pull (LED)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // PA1 as Input Pull-up
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    
}

int main()
{
    GPIO_Config();
    GPIO_ResetBits(GPIOC, GPIO_Pin_13); // Led ON

    while (1)
    {
			   // check if button (PA1) is pressed (active low)
        if (0U == GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1))
        {
            Delay_Ms(100U); // Decounce delay (100ms)
           
					  // check button dung dang nhan khong
            if (0U == GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1))
            {
							 // wait until button is released
                while (0U == GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1));
                u32Count++;
                if (u32Count % 2 == 0)
                {
                    GPIO_ResetBits(GPIOC, GPIO_Pin_13);
                }
                else
                {
                    GPIO_SetBits(GPIOC, GPIO_Pin_13);
                }
            }
        }
    }
}

