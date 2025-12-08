#include "stm32f10x.h"
#include <stdint.h>

/* ===================== CONFIG ===================== */
#define LCD_RS   (1U << 2)   
#define LCD_EN   (1U << 3)   
#define LCD_D4   (1U << 4)   
#define LCD_D5   (1U << 5)   
#define LCD_D6   (1U << 6)   
#define LCD_D7   (1U << 7)   
#define LCD_PORT GPIOA

/* ===================== FUNCTION PROTOTYPES ===================== */
static void Clock_Init(void);
static void SysTick_Init(void);
static void DelayMs(uint32_t ms);
static void LCD_GPIO_Init(void);
static void LCD_Init(void);
static void LCD_Enable(void);
static void LCD_Send4Bit(uint8_t data);
static void LCD_SendCmd(uint8_t cmd);
static void LCD_SendData(uint8_t data);

static void LCD_SetCursor(int8_t row, int8_t col);
static void LCD_Print(const char *str);

/* ===================== GLOBAL ===================== */
static volatile uint32_t msTicks = 0;

/* ===================== MAIN ===================== */
int main(void)
{
	  
    Clock_Init();
	
	SystemCoreClockUpdate();
    SysTick_Init();
    LCD_Init();


    LCD_SetCursor(0, 0);
    LCD_Print("STM32 LCD 20x4");

    LCD_SetCursor(1, 0);
    LCD_Print("GPIO Mode Demo");

    LCD_SetCursor(2, 0);
    LCD_Print("No I2C Needed!");

    LCD_SetCursor(3, 0);
    LCD_Print("4-bit OK!");
	
	 

    while (1)
    {
        // Main loop
    }
}

/* ===================== CLOCK ===================== */
static void Clock_Init(void)
{
    RCC->CR |= (1U << 16);                     // HSEON = 1
    while (!(RCC->CR & (1U << 17)));           // Wait HSERDY
    FLASH->ACR |= (1U << 4) | (1U << 1);       // PRFTBE=1, LATENCY=1
    RCC->CFGR |= (1U << 16) | (7U << 18);      // PLLSRC=HSE, PLLMUL=9
    RCC->CFGR |= (4U << 8);                    // APB1 = /2
    RCC->CR |= (1U << 24);                     // PLLON=1
    while (!(RCC->CR & (1U << 25)));           // Wait PLLRDY
    RCC->CFGR |= (2U << 0);                    // SW=PLL
	
	RCC->CFGR &= ~(3U << 0);
    while (((RCC->CFGR >> 2) & 3U) != 2U);     // Wait PLL selected
}

/* ===================== SYSTICK ===================== */
static void SysTick_Init(void)
{
    SysTick->LOAD = (SystemCoreClock / 1000U) - 1;
    SysTick->VAL  = 0; // reset bo dem hien tai
    SysTick->CTRL = 7; // Enable SysTick, interrupt, use CPU clock
}

void SysTick_Handler(void)
{
    msTicks++;
}

static inline void DelayMs(uint32_t ms)
{
    uint32_t start = msTicks;
    while ((uint32_t)(msTicks - start) < ms); // phai dung uint32_t tranh error khi tran
}

/* ===================== LCD LOW LEVEL ===================== */
static void LCD_GPIO_Init(void)
{
  
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    
    LCD_PORT->CRL &= ~(
        (0xF << (2 * 4)) | // PA2
        (0xF << (3 * 4)) | // PA3
        (0xF << (4 * 4)) | // PA4
        (0xF << (5 * 4)) | // PA5
        (0xF << (6 * 4)) | // PA6
        (0xF << (7 * 4))   // PA7
    );
	
	
    // Output 50MHz push-pull
    LCD_PORT->CRL |= (0x3 << (2*4)) | (0x3 << (3*4)) | (0x3 << (4*4)) | (0x3 << (5*4)) | (0x3 << (6*4)) | (0x3 << (7*4));

    // reset ve 0
    LCD_PORT->BRR = (LCD_RS | LCD_EN | LCD_D4 | LCD_D5 | LCD_D6 | LCD_D7);
}

static void LCD_Enable(void)
{
    LCD_PORT->BSRR = LCD_EN;     // EN = 1
    DelayMs(1);
    LCD_PORT->BRR  = LCD_EN;     // EN = 0
    DelayMs(1);
}

static void LCD_Send4Bit(uint8_t data)
{
    // Clear D4–D7
    LCD_PORT->BRR = LCD_D4 | LCD_D5 | LCD_D6 | LCD_D7;

    
    if (data & (1 << 4)) LCD_PORT->BSRR = LCD_D4;
    if (data & (1 << 5)) LCD_PORT->BSRR = LCD_D5;
    if (data & (1 << 6)) LCD_PORT->BSRR = LCD_D6;
    if (data & (1 << 7)) LCD_PORT->BSRR = LCD_D7;

    LCD_Enable();
}

static void LCD_SendCmd(uint8_t cmd)
{
    LCD_PORT->BRR = LCD_RS;            // RS = 0  Gui lenh
    LCD_Send4Bit(cmd & 0xF0);          // Gui 4 bit cao
    LCD_Send4Bit((cmd << 4) & 0xF0);   // Gui 4 bit thap
    DelayMs(2);
}

static void LCD_SendData(uint8_t data)
{
    LCD_PORT->BSRR = LCD_RS;     
    LCD_Send4Bit(data & 0xF0);
    LCD_Send4Bit(data << 4);
    DelayMs(2);
}

/* ===================== LCD HIGH LEVEL ===================== */
static void LCD_SetCursor(int8_t row, int8_t col)
{
    uint8_t pos;

    if (row < 0) row = 0;
    if (col < 0) col = 0;
    if (row > 3) row = 3;
    if (col > 19) col = 19;

    switch (row)
    {
        case 0: pos = 0x80 + col; break;
        case 1: pos = 0xC0 + col; break;
        case 2: pos = 0x94 + col; break;
        case 3: pos = 0xD4 + col; break;
        default: pos = 0x80; break;
    }
    LCD_SendCmd(pos);
}

static void LCD_Print(const char *str)
{
    while (*str) LCD_SendData(*str++);
}

/* ===================== LCD INIT ===================== */
static void LCD_Init(void)
{
    LCD_GPIO_Init();
    DelayMs(50);
	
    LCD_Send4Bit(0x30);
    DelayMs(5);
    LCD_Send4Bit(0x30);
    DelayMs(5);
    LCD_Send4Bit(0x30);
    DelayMs(5);
    LCD_Send4Bit(0x20); // Chuyen sang 4-bit mode
	
	DelayMs(2);

    LCD_SendCmd(0x28); // 4-bit, 2 line, 5x8 font
    LCD_SendCmd(0x0C); // Display ON, Cursor OFF
    LCD_SendCmd(0x06); // Entry mode -> cursor tu dong di chuyen
    LCD_SendCmd(0x01); // Clear display
    DelayMs(5);
}
