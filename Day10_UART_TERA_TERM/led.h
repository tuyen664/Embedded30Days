#ifndef LED_H
#define LED_H
#include "stm32f10x.h"
#include <stdint.h>

#define LED_PIN   13
#define LED_PORT  GPIOC

void LED_Init(void);
void LED_ON(void);
void LED_OFF(void);
void LED_TOGGLE(void);
uint8_t IS_LED_ON(void);

#endif
