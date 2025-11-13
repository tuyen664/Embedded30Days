#include "led.h"

static volatile uint8_t ledState = 0;

void LED_Init(void)
{
    RCC->APB2ENR |= (1 << 4);
    GPIOC->CRH &= ~(0xF << 20);
    GPIOC->CRH |=  (0x2 << 20);
    LED_ON();
}

void LED_ON(void)  { LED_PORT->BRR  = (1 << LED_PIN); ledState = 1; }
void LED_OFF(void) { LED_PORT->BSRR = (1 << LED_PIN); ledState = 0; }
void LED_TOGGLE(void)
{
    if (ledState) LED_OFF(); else LED_ON();
}
uint8_t IS_LED_ON(void) { return ledState; }
