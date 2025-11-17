#include "stm32f10x.h"
#include <string.h>
#include <stdio.h>

/* ===================== CONFIG ===================== */
#define LCD_BACKLIGHT   0x08 // bit 3
#define LCD_ENABLE      0x04 // bit 2
#define LCD_RS          0x01 // bit 0 -> Register Select Command / Data

#define LCD_ROW_0       0
#define LCD_ROW_1       1
#define LCD_ROW_2       2
#define LCD_ROW_3       3

#define I2C_TIMEOUT_MS  100
#define LCD_ADDR_1      (0x27 << 1)
#define LCD_ADDR_2      (0x3F << 1)

/* ===================== FUNCTION PROTOTYPES ===================== */
static void Clock_Init(void);
static void SysTick_Init(void);
static void DelayMs(uint32_t ms);
static void UART1_Init(void);
static void UART1_SendString(const char *s);

static void I2C1_Init(void);
static uint8_t I2C1_Write(uint8_t addr, uint8_t *data, uint8_t len);

static uint8_t LCD_AutoDetectAddr(void);
static void LCD_Init(uint8_t addr);
static void LCD_SendCmd(uint8_t addr, uint8_t cmd);
static void LCD_SendData(uint8_t addr, uint8_t data);
static void LCD_SendString(uint8_t addr, const char *str);
static void LCD_SetCursor(uint8_t addr, int8_t row, int8_t col);
static void LCD_Clear(uint8_t addr);

/* ===================== GLOBAL ===================== */
static volatile uint32_t msTicks = 0;

/* ===================== MAIN ===================== */
int main(void)
{
    uint8_t lcd_addr;

    Clock_Init();
    SysTick_Init();
    UART1_Init();
    I2C1_Init();

    UART1_SendString("\r\n[SYS] Booting...\r\n");
    DelayMs(100);

    lcd_addr = LCD_AutoDetectAddr();
    if (!lcd_addr) {
        UART1_SendString("[ERR] LCD not found on I2C bus!\r\n");
        while (1);
    }

    char msg[64];
    sprintf(msg, "[OK] LCD detected at 0x%02X\r\n", lcd_addr);
    UART1_SendString(msg);

    LCD_Init(lcd_addr);
    LCD_Clear(lcd_addr);
    LCD_SetCursor(lcd_addr, 0, 0);
    LCD_SendString(lcd_addr, "LCD V2.0");
    LCD_SetCursor(lcd_addr, 1, 0);
    LCD_SendString(lcd_addr, "Auto Detect OK!");

    while (1)
    {
        DelayMs(1000);
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
    while (((RCC->CFGR >> 2) & 3U) != 2U);     // Wait PLL selected
}

/* ===================== SYSTICK ===================== */
static void SysTick_Init(void)
{
    SysTick->LOAD = (SystemCoreClock / 1000U) - 1;
    SysTick->VAL  = 0; // reset bo dem hien tai
    SysTick->CTRL = 7;
}
void SysTick_Handler(void) { msTicks++; }
static void DelayMs(uint32_t ms)
{
    uint32_t start = msTicks;
    while ( (uint32_t)(msTicks - start) < ms);
}

/* ===================== UART DEBUG ===================== */
static void UART1_Init(void)
{
    RCC->APB2ENR |= (1U << 0) | (1U << 2) | (1U << 14);
    GPIOA->CRH &= ~((0xFU << 4) | (0xFU << 8));
    GPIOA->CRH |=  (0x0BU << 4) | (0x04U << 8);
    USART1->BRR = 0x1D4C;  // 72MHz / 9600
    USART1->CR1 = (1U << 13) | (1U << 3) | (1U << 2);
}
static void UART1_SendString(const char *s)
{
    while (*s) {
        while (!(USART1->SR & (1U << 7)));
        USART1->DR = *s++;
    }
}

/* ===================== I2C1 INIT ===================== */
static void I2C1_Init(void)
{
    RCC->APB2ENR |= (1U << 3);   // GPIOBEN
    RCC->APB1ENR |= (1U << 21);  // I2C1EN
    GPIOB->CRL &= ~((0xFU << 24) | (0xFU << 28));
    GPIOB->CRL |=  (0xFU << 24) | (0xFU << 28); // PB6, PB7 AF-OD 50MHz
    I2C1->CR1 = (1U << 15);  // SWRST = 1  -> reset
    I2C1->CR1 = 0;          // SWRST = 0 -> nha nut reset
    I2C1->CR2 = 36;          // 36MHz = APB1 (PCLK1)
    I2C1->CCR = 180;         // SCL = 100kHz ; CCR = PCLK1 / (2 * Fscl)
    I2C1->TRISE = 37;  // thoi gian troi len toi da 1000ns  TRISE = (PCLK1 * 1000ns) + 1
    I2C1->CR1 |= (1U << 0);  //  Peripheral Enable
}

/* ===================== I2C WRITE (with timeout) ===================== */
static uint8_t I2C1_Write(uint8_t addr, uint8_t *data, uint8_t len)
{
    uint32_t t0;

    I2C1->CR1 |= (1U << 8); // START
    t0 = msTicks; while (!(I2C1->SR1 & (1U << 0))) 
	
	  if ((msTicks - t0) > I2C_TIMEOUT_MS) return 0;

    I2C1->DR = addr; // send address -> clear Start bit
    t0 = msTicks;
	
   while (!(I2C1->SR1 & (1U << 1))) // wait ADDR (Address sent)
	  {
	  if ((msTicks - t0) > I2C_TIMEOUT_MS) return 0;
	  if (I2C1->SR1 & (1U << 10))  // AF bit (bit 10)
    {
        I2C1->CR1 |= (1U << 9);  // Goi STOP
        (void)I2C1->SR1; 
        (void)I2C1->SR2;          // Ðoc de clear ADDR/AF
        return 0;                 // Bao loi
    }
			
    }
	
	
    (void)I2C1->SR2; // -> clear ADDR

    for (uint8_t i = 0; i < len; i++)
    {
        t0 = msTicks; 
			while (!(I2C1->SR1 & (1U << 7))) // TXE kiem tra DR trong moi duoc ghi vao
			 { 
			 if ((msTicks - t0) > I2C_TIMEOUT_MS) return 0;
			 }
			
        I2C1->DR = data[i];
    }

    t0 = msTicks; 
		while (!(I2C1->SR1 & (1U << 2))) // BTF - du lieu truyen Finished
    {
		if ((msTicks - t0) > I2C_TIMEOUT_MS) return 0;
		}
    I2C1->CR1 |= (1U << 9); // STOP
    return 1;
}

/* ===================== LCD CORE ===================== */
static void LCD_SendCmd(uint8_t addr, uint8_t cmd)
{
	
	// bien byte lenh cmd thành 4 gói du lieu I2C  gui qua chip PCF8574.
	
    uint8_t data[4];  // 4byte
    uint8_t upper = cmd & 0xF0; // 4 bit hight
    uint8_t lower = (cmd << 4) & 0xF0;  // 4 bit low
    data[0] = upper | LCD_BACKLIGHT | LCD_ENABLE;   // E=1
    data[1] = upper | LCD_BACKLIGHT;                // E=0
    data[2] = lower | LCD_BACKLIGHT | LCD_ENABLE;   // E=1
    data[3] = lower | LCD_BACKLIGHT;                // E=0
    I2C1_Write(addr, data, 4); // ghi 4 byte 
    DelayMs(2); // nhieu lenh den 1.5ms
}

static void LCD_SendData(uint8_t addr, uint8_t data_)
{
	
	//Gui mot ky tu hien thi (data byte) tu STM32 qua I2C toi LCD 
	
    uint8_t data[4];
    uint8_t upper = data_ & 0xF0;
    uint8_t lower = (data_ << 4) & 0xF0;
    data[0] = upper | LCD_BACKLIGHT | LCD_ENABLE | LCD_RS;
    data[1] = upper | LCD_BACKLIGHT | LCD_RS;
    data[2] = lower | LCD_BACKLIGHT | LCD_ENABLE | LCD_RS;
    data[3] = lower | LCD_BACKLIGHT | LCD_RS;
    I2C1_Write(addr, data, 4);
    DelayMs(2);
}

/* ===================== LCD HIGH LEVEL ===================== */
static void LCD_Clear(uint8_t addr)
{
    LCD_SendCmd(addr, 0x01);
    DelayMs(2);
}

static void LCD_SetCursor(uint8_t addr, int8_t row, int8_t col)
{
    uint8_t pos;
	
	  if (row < 0) row = 0;
    if (col < 0) col = 0;
	  if (row > 3) row = 0;         
    if (col > 19) col = 19;       
	
    switch (row)
    {
        case 0: pos = 0x80 + col; break; 
        case 1: pos = 0xC0 + col; break;
        case 2: pos = 0x94 + col; break;
        case 3: pos = 0xD4 + col; break;
    }
    LCD_SendCmd(addr, pos);
}

static void LCD_SendString(uint8_t addr, const char *str)
{
    while (*str) LCD_SendData(addr, *str++);
}

/* ===================== LCD INIT ===================== */
static void LCD_Init(uint8_t addr)
{
    DelayMs(50);
    LCD_SendCmd(addr, 0x30);
    DelayMs(5);
    LCD_SendCmd(addr, 0x30);
    DelayMs(1);
    LCD_SendCmd(addr, 0x30);
    DelayMs(10);
    LCD_SendCmd(addr, 0x20); // chuyen sang 4-bit mode
    LCD_SendCmd(addr, 0x28); // 4-bit, 2 dong logic , font 5x8
    LCD_SendCmd(addr, 0x08); // xoa rac bo nho hien thi Display OFF
    LCD_SendCmd(addr, 0x01); // Clear Display
    DelayMs(2);
    LCD_SendCmd(addr, 0x06); // sau khi ghi 1 ki tu , con tro tu dong nhay sang phai
    LCD_SendCmd(addr, 0x0C); // 0 0 0 0 1 D C B -> 1100
	// Bit 2 (D): Display ON/OFF
  // Bit 1 (C): Cursor ON/OFF
  // Bit 0 (B): Blink ON/OFF nhay con tro
}

/* ===================== AUTO-DETECT I2C ADDRESS ===================== */
static uint8_t LCD_AutoDetectAddr(void)
{
    uint8_t dummy = 0x00;
    if (I2C1_Write(LCD_ADDR_1, &dummy, 1))
        return LCD_ADDR_1;
    else if (I2C1_Write(LCD_ADDR_2, &dummy, 1))
        return LCD_ADDR_2;
    else
        return 0;
}
