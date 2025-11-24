#include "stm32f10x.h"
#include <string.h>  
#include <stdint.h> 
#include <stddef.h> 

/* ============================
   STM32 SPI1 MASTER – FULL
   ============================ */

/* ---------- Pin definitions ---------- */
#define LED_PORT       GPIOC
#define LED_PIN        13

#define SPI1_CS1_PORT  GPIOA
#define SPI1_CS1_PIN   4
#define SPI1_CS2_PORT  GPIOA
#define SPI1_CS2_PIN   3

#define CS_LOW(PORT,PIN)   ((PORT)->BSRR = (1 << ((PIN)+16)))
#define CS_HIGH(PORT,PIN)  ((PORT)->BSRR = (1 << (PIN)))

/* ---------- Constants ---------- */
#define SPI_TIMEOUT 10000

/* ---------- DMA channels ---------- */
#define SPI_DMA_RX_CH DMA1_Channel2
#define SPI_DMA_TX_CH DMA1_Channel3

/* ---------- Globals ---------- */
volatile uint8_t spiTransferDone = 0;
volatile uint8_t spiError = 0;

/* ---------- Function Prototypes ---------- */
/* LED & Delay */
void LED_Init(void);
void LED_Set(uint8_t state);
void Delay_ms(uint32_t ms);

/* CS */
void CS_Init(GPIO_TypeDef* port, uint8_t pin);
void CS_Select(GPIO_TypeDef* port, uint8_t pin);
void CS_Deselect(GPIO_TypeDef* port, uint8_t pin);

/* SPI1 */
void SPI1_Init(void);
uint8_t SPI1_TransferByte(uint8_t data);
void SPI1_TransferBuffer(uint8_t* txBuf, uint8_t* rxBuf, uint16_t len, GPIO_TypeDef* csPort, uint8_t csPin);
void SPI1_TransferBuffer_DMA(uint8_t* txBuf, uint8_t* rxBuf, uint16_t len, GPIO_TypeDef* csPort, uint8_t csPin);

/* DMA IRQ */
void DMA1_Channel2_IRQHandler(void);
void DMA1_Channel3_IRQHandler(void);


int main(void)
{
    LED_Init();
    SPI1_Init();
    CS_Init(SPI1_CS1_PORT,SPI1_CS1_PIN);
    CS_Init(SPI1_CS2_PORT,SPI1_CS2_PIN);

    static uint8_t tx1[8] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
    static uint8_t rx1[8] = {0};
    static uint8_t tx2[8] = {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,0x00,0x99};
    static uint8_t rx2[8] = {0};

    while(1)
    { 
        /* Polling transfer */
			  memset(rx1,0,sizeof(rx1));
        SPI1_TransferBuffer(tx1,rx1,8,SPI1_CS1_PORT,SPI1_CS1_PIN);
        
        
        /* DMA transfer */
			  memset(rx2,0,sizeof(rx2));
			
			  spiTransferDone = 0;// bat buoc
        spiError = 0;       // bat buoc
        
        SPI1_TransferBuffer_DMA(tx2,rx2,8,SPI1_CS2_PORT,SPI1_CS2_PIN);

		  	while(!spiTransferDone && !spiError)
        { 
				
				} ;
				
			
        /* Check result */
        uint8_t ok = 1;
        for(int i=0;i<8;i++)
        {
            if(rx1[i]!=tx1[i] || rx2[i]!=tx2[i])
            {
                ok=0;
                break;
            }
        }

        LED_Set(ok);   // LED Off if OK, On if error
        Delay_ms(200);
    }
}

/* ============================
   LED & Delay
   ============================ */

void LED_Set(uint8_t state)
{
    if(state) LED_PORT->BSRR = (1 << (LED_PIN+16)); // ON
    else LED_PORT->BSRR = (1 << LED_PIN);          // OFF
}
void LED_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    LED_PORT->CRH &= ~(0x0F << 20);
    LED_PORT->CRH |= (0x03 << 20); // Output push-pull
    LED_Set(0); // OFF
}


void Delay_ms(uint32_t ms)
{
    for(uint32_t i=0;i<ms*8000;i++) __NOP();
}

/* ============================
   CS
   ============================ */
void CS_Init(GPIO_TypeDef* port, uint8_t pin)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    uint32_t pos = pin*4;
    port->CRL &= ~(0x0F << pos);
    port->CRL |= (0x03 << pos); // Output push-pull
    CS_HIGH(port,pin);
}

void CS_Select(GPIO_TypeDef* port, uint8_t pin)
{
    CS_LOW(port,pin);
}

void CS_Deselect(GPIO_TypeDef* port, uint8_t pin)
{
    CS_HIGH(port,pin);
}

/* ============================
   SPI1 Init
   ============================ */
void SPI1_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_SPI1EN;
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    /* SPI1 pins PA5=SCK, PA6=MISO, PA7=MOSI */
    GPIOA->CRL &= ~((0x0F<<20)|(0x0F<<24)|(0x0F<<28));
    GPIOA->CRL |= ((0x0B<<20)|(0x04<<24)|(0x0B<<28)); // SCK & MOSI AF push-pull, MISO input

    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_SPE | (0x3 << 3); // BR=16
}

/* ============================
   SPI1 Byte transfer polling
   ============================ */


uint8_t SPI1_TransferByte(uint8_t data)
{
    uint32_t timeout;
    volatile uint8_t dummy;

    /* -------------------------------------------------------
       1) Xoa toan bo du lieu con ton trong RX buffer
       ------------------------------------------------------- */
    while (SPI1->SR & SPI_SR_RXNE)
    {
        dummy = SPI1->DR;      // doc de clear RXNE
    }
    (void)dummy;               // tranh warning bien khong dung

    /* -------------------------------------------------------
       2) Cho SPI thuc su khong ban (BSY = 0)
       ------------------------------------------------------- */
    timeout = SPI_TIMEOUT;
    while (SPI1->SR & SPI_SR_BSY)
    {
        if (--timeout == 0)
            return 0xFF;       // loi: SPI dang ban
    }

    /* -------------------------------------------------------
       3) Cho TXE = 1 de ghi du lieu moi vao DR
       ------------------------------------------------------- */
    timeout = SPI_TIMEOUT;
    while (!(SPI1->SR & SPI_SR_TXE))
    {
        if (--timeout == 0)
            return 0xFE;       // loi: TXE timeout
    }

    SPI1->DR = data;           // ghi byte can gui

    /* -------------------------------------------------------
       4) Cho den khi byte nhan ve ton tai trong RXNE
       ------------------------------------------------------- */
    timeout = SPI_TIMEOUT;
    while (!(SPI1->SR & SPI_SR_RXNE))
    {
        if (--timeout == 0)
            return 0xFD;       // loi: RXNE timeout
    }

    uint8_t recv = SPI1->DR;   // doc byte da nhan

    /* -------------------------------------------------------
       5) Kiem tra loi Overrun (OVR)
       ------------------------------------------------------- */
    if (SPI1->SR & SPI_SR_OVR)
    {
        dummy = SPI1->DR;  // phai doc DR de clear OVR
        dummy = SPI1->SR;  // doc SR de clear OVR
        (void)dummy;

        return 0xFC;       // loi: OVR
    }

    /* -------------------------------------------------------
       6) Cho BSY = 0 de dam bao qua trinh clock ket thuc
       ------------------------------------------------------- */
    timeout = SPI_TIMEOUT;
    while (SPI1->SR & SPI_SR_BSY)
    {
        if (--timeout == 0)
            return 0xFB;   // loi: ket thuc truyen nhung BSY van = 1
    }

    return recv;            // tra ve byte nhan duoc
}



/* ============================
   SPI1 Multi-byte transfer polling
   ============================ */
void SPI1_TransferBuffer(uint8_t* txBuf, uint8_t* rxBuf, uint16_t len, GPIO_TypeDef* csPort, uint8_t csPin)
{
    CS_Select(csPort,csPin);
    for(uint16_t i=0;i<len;i++)
    {
        uint8_t data = SPI1_TransferByte(txBuf[i]);
        if(rxBuf) rxBuf[i] = data;
    }
    CS_Deselect(csPort,csPin);
}

/* ============================
   DMA IRQ Handlers
   ============================ */
void DMA1_Channel2_IRQHandler(void)  // RX
{
    if(DMA1->ISR & DMA_ISR_TCIF2)
    {
        DMA1->IFCR = DMA_IFCR_CTCIF2;
        spiTransferDone = 1;
    }
		if (DMA1->ISR & DMA_ISR_TEIF2)
    {
        DMA1->IFCR = DMA_IFCR_CTEIF2;   // Clear TE flag
    }
}

void DMA1_Channel3_IRQHandler(void)  // TX
{
    if(DMA1->ISR & DMA_ISR_TCIF3)
    {
        DMA1->IFCR = DMA_IFCR_CTCIF3;
    }
		if (DMA1->ISR & DMA_ISR_TEIF3)
    {
        DMA1->IFCR = DMA_IFCR_CTEIF3;   // Clear TE flag

    }
}

/* ============================
   SPI1 Multi-byte transfer DMA
   ============================ */
void SPI1_TransferBuffer_DMA(uint8_t* txBuf, uint8_t* rxBuf, uint16_t len, GPIO_TypeDef* csPort, uint8_t csPin)
{
    spiTransferDone = 0;
    spiError = 0;
	  if(txBuf == NULL || rxBuf == NULL)
    return;
		// Clear cac flag DMA cu truoc khi bat dau
		DMA1->IFCR = DMA_IFCR_CTCIF2 | DMA_IFCR_CTEIF2 
                | DMA_IFCR_CTCIF3 | DMA_IFCR_CTEIF3;

    CS_Select(csPort,csPin);

    /* TX DMA */
    SPI_DMA_TX_CH->CCR = 0;
    SPI_DMA_TX_CH->CPAR = (uint32_t)&(SPI1->DR);
    SPI_DMA_TX_CH->CMAR = (uint32_t)txBuf;
    SPI_DMA_TX_CH->CNDTR = len;
    SPI_DMA_TX_CH->CCR |= (1 << 0)   // EN
                        | (1 << 1)   // TCIE
                        | (1 << 6)   // MINC
                        | (1 << 7);  // DIR

    /* RX DMA */
    SPI_DMA_RX_CH->CCR = 0;
    SPI_DMA_RX_CH->CPAR = (uint32_t)&(SPI1->DR);
    SPI_DMA_RX_CH->CMAR = (uint32_t)rxBuf;
    SPI_DMA_RX_CH->CNDTR = len;
    SPI_DMA_RX_CH->CCR |= (1 << 0)   // EN
                        | (1 << 1)   // TCIE
                        | (1 << 6);  // MINC
												
		// Kiem tra SPI khong ban truoc khi enable DMA										
    while(SPI1->SR & SPI_SR_BSY);
    /* Enable SPI DMA */
    SPI1->CR2 |= SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN;
		


    /* NVIC IRQ */
		NVIC_SetPriority(DMA1_Channel2_IRQn, 2);
    NVIC_SetPriority(DMA1_Channel3_IRQn, 2);
    NVIC_EnableIRQ(DMA1_Channel2_IRQn);
    NVIC_EnableIRQ(DMA1_Channel3_IRQn);

    /* Wait for RX DMA complete */
    uint32_t timeout = SPI_TIMEOUT*10;
    while(!spiTransferDone && !spiError)
        if(--timeout==0){ spiError=1; break; }

    /* Disable SPI DMA */
    SPI1->CR2 &= ~(SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN);
    CS_Deselect(csPort,csPin);
}

