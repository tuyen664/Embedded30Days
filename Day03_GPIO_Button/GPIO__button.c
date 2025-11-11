#include "stm32f10x.h"

uint32_t  u32Count = 0u;


void Delay_Ms(uint32_t u32Delay)
{
	while(u32Delay)
	{
		SysTick->LOAD = 72U*1000U - 1U;
		SysTick->VAL = 0U;
		SysTick->CTRL =5U;
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

    // PC13 là Output Push-Pull (LED)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // PA1 là Input Pull-up
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    
}

int main()
{
    GPIO_Config();
    GPIO_SetBits(GPIOC, GPIO_Pin_13);

    while (1)
    {
        if (0U == GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1))
        {
            Delay_Ms(100U);

            if (0U == GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1))
            {
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

