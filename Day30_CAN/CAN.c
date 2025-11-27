#include "stm32f10x.h"
#include <stdint.h>

/* ================== CLOCK CONFIG ================== */
#define SYSCLK_HZ   72000000UL
#define PCLK1_HZ    (SYSCLK_HZ/2)     /* 36 MHz */
#define CAN_TX_TIMEOUT 100000U

/* ================== UART ================== */

static void UART1_Init(uint32_t baud)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_USART1EN;

    /* PA9 TX: AF-PP, 50 MHz */
    GPIOA->CRH &= ~(0xF << 4);
    GPIOA->CRH |=  (0xB << 4);

    /* PA10 RX: input floating */
    GPIOA->CRH &= ~(0xF << 8);
    GPIOA->CRH |=  (0x4 << 8);

    /* Baud = fclk / baud */
    USART1->BRR = (SYSCLK_HZ + baud / 2) / baud;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

static void UART1_TxChar(char c)
{
    while (!(USART1->SR & USART_SR_TXE));
    USART1->DR = c;
}

static void UART1_TxStr(const char *s)
{
    while (*s) UART1_TxChar(*s++);
}

static void UART1_TxHex8(uint8_t v)
{
    const char *h = "0123456789ABCDEF";
    UART1_TxChar(h[v >> 4]);
    UART1_TxChar(h[v & 0xF]);
}

static void UART1_TxHex16(uint16_t v)
{
    UART1_TxHex8(v >> 8);
    UART1_TxHex8(v & 0xFF);
}

/* ================== CAN REG-LEVEL LOOPBACK ================== */

volatile uint8_t  can_rx_flag = 0;
volatile uint16_t can_rx_id   = 0;
volatile uint8_t  can_rx_len  = 0;
volatile uint8_t  can_rx_buf[8];

static void CAN_GPIO_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;

    /* Remap PB8/PB9 */
    AFIO->MAPR &= ~(3 << 13);
    AFIO->MAPR |=  (2 << 13); // remap tu PA11/PA12 -> PB8/PB9 

    /* PB8 = CANRX = input floating */
    GPIOB->CRH &= ~(0xF << 0);
    GPIOB->CRH |=  (0x4 << 0);

    /* PB9 = CANTX = AF PP 50MHz */
    GPIOB->CRH &= ~(0xF << 4);
    GPIOB->CRH |=  (0xB << 4);
}

static void CAN_Setup(void)
{
	
	 
	  RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;
    __DSB();
	
    /* Reset CAN */
    RCC->APB1RSTR |=  RCC_APB1RSTR_CAN1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_CAN1RST;
  
	  // Enter init mode
    CAN1->MCR |= CAN_MCR_INRQ;
    while (!(CAN1->MSR & CAN_MSR_INAK));

    /* Baud 500 kbps: tq=18, PCLK1=36MHz , BRP=4 */
	  /* 36000000/ (4*(1+5+12)) = 500000*/
    CAN1->BTR = 
        (3 << 0) |       // BRP = 4 tq
        (11 << 16) |      // TS1 = 12 tq 
        (4 << 20) |      // TS2 = 5 tq 
        (0  << 24);       // SJW = 1 tq

    /* Loopback mode */
    CAN1->BTR |= CAN_BTR_LBKM;
	
	  CAN1->MCR &= ~CAN_MCR_NART; // Auto retransmission enable
    CAN1->MCR |= CAN_MCR_ABOM;  // Automatic bus-off recovery
    

    /* Filter 0 accept all into FIFO0 */
    CAN1->FMR |= CAN_FMR_FINIT;  // vao filter init mode
    CAN1->FA1R = 0;              // disable filter 0
    CAN1->FM1R &= ~(1 << 0);     /* mask mode */
    CAN1->FS1R |=  (1 << 0);     /* 32-bit */
    CAN1->FFA1R &= ~(1 << 0);    // FIFO 0
    CAN1->sFilterRegister[0].FR1 = 0; // filter ID   ((0x100 << 21);)
    CAN1->sFilterRegister[0].FR2 = 0; // Mask ((0x7FF << 21); )
    CAN1->FA1R |= (1 << 0);           // enable filter 0
    CAN1->FMR &= ~CAN_FMR_FINIT;      // exit filter init mode

    CAN1->MCR &= ~CAN_MCR_SLEEP;
		
    while (CAN1->MSR & CAN_MSR_SLAK);
		
    CAN1->MCR &= ~CAN_MCR_INRQ;
		
    uint32_t timeout = 0; 
    while (CAN1->MSR & CAN_MSR_INAK)
	{
    if (++timeout > 1000000) 
		{
        UART1_TxStr("[CAN] ERROR: INAK not cleared (leave init failed)\r\n");
        break;
    }
  };
	
	  /* Enable RX FIFO0 interrupt */
    CAN1->IER |= CAN_IER_FMPIE0;
    NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);

		
		 if (CAN1->MSR & CAN_MSR_ERRI)
    {
        // if error , add  UART print
    }
}

static int CAN_Send(uint16_t id, uint8_t *data, uint8_t len)
{
    if (!data || len > 8) return 0;

    uint32_t tsr; // transmit status register
    uint8_t box;  // mailbox
 
	  // cho mailbox trong , CAN co 3 mailbox TME0 , TME1 , TME2
	  uint32_t t=0;
    do {
        tsr = CAN1->TSR;
			  t++;
			  if (t >= CAN_TX_TIMEOUT) 
				{ 
            UART1_TxStr("[CAN] Mailbox busy, TX skipped\r\n");
            return 0;
        }
			  
       } while (!((tsr & CAN_TSR_TME0) || (tsr & CAN_TSR_TME1) || (tsr & CAN_TSR_TME2)));

		
		// chon mailbox ranh dau tien
    if (tsr & CAN_TSR_TME0) box = 0;
    else if (tsr & CAN_TSR_TME1) box = 1;
    else box = 2;
 
		// lay con tro toi mailbox
    CAN_TxMailBox_TypeDef *mb = &CAN1->sTxMailBox[box];

	
		// reset TIR de tranh bit cu
    mb->TIR = 0;  
			 
    // gán ID 11-bit
    mb->TIR  = (id & 0x7FF) << 21;
			 
    mb->TDTR = len & 0xF; // DLC

		// pack data vào DLR/DHR
    uint32_t dl = 0, dh = 0;
    for (int i=0;i<4 && i<len;i++) dl |= data[i] << (i*8); // DLR
    for (int i=4;i<8 && i<len;i++) dh |= data[i] << ((i-4)*8); // DLH

    mb->TDLR = dl;
    mb->TDHR = dh;

	 // dat co TX Request -> CAN  bat dau gui Frame
    mb->TIR |= CAN_TI0R_TXRQ;
		
		// cho TX hoan tat (TXOK) voi timeout
    t = 0;
    uint32_t txok_mask = (CAN_TSR_TXOK0 << (box*8));
    while (!(CAN1->TSR & txok_mask)) 
		{
        t++;
        if (t >= CAN_TX_TIMEOUT) 
				{
            UART1_TxStr("[CAN] TX timeout\r\n");
            return 0;
        }
    }
		
    return 1;
}

/* ================== CAN RX RING BUFFER ================== */
#define CAN_RX_RING_SIZE 16

typedef struct {
    uint32_t id;       // Standard or Extended ID
    uint8_t  len;      // DLC
    uint8_t  data[8];
    uint8_t  ide;      // 0=Standard, 1=Extended
} CAN_Frame_t;

volatile CAN_Frame_t can_rx_ring[CAN_RX_RING_SIZE];
volatile uint8_t can_rx_head = 0;
volatile uint8_t can_rx_tail = 0;

void USB_LP_CAN1_RX0_IRQHandler(void)
{
    // Xu ly tat ca frame co trong FIFO0
    while (CAN1->RF0R & CAN_RF0R_FMP0)
    {
        uint32_t RIR  = CAN1->sFIFOMailBox[0].RIR;
        uint32_t RDTR = CAN1->sFIFOMailBox[0].RDTR;
        uint32_t RDLR = CAN1->sFIFOMailBox[0].RDLR;
        uint32_t RDHR = CAN1->sFIFOMailBox[0].RDHR;

       volatile CAN_Frame_t *frame = &can_rx_ring[can_rx_head];

        // Kiem tra Standard / Extended ID
        frame->ide = (RIR & CAN_RI0R_IDE) ? 1 : 0;
        if (frame->ide)
            frame->id = (RIR >> 3) & 0x1FFFFFFF;  // Extended 29-bit
        else
            frame->id = (RIR >> 21) & 0x7FF;      // Standard 11-bit

        frame->len = RDTR & 0xF;

        // Copy du lieu 8 byte
        for (uint8_t i = 0; i < frame->len && i < 4; i++)
            frame->data[i] = (RDLR >> (i*8)) & 0xFF;
        for (uint8_t i = 4; i < frame->len && i < 8; i++)
            frame->data[i] = (RDHR >> ((i-4)*8)) & 0xFF;

        // Cap nhat ring buffer
        can_rx_head = (can_rx_head + 1) % CAN_RX_RING_SIZE; // khi can_rx_head =15 , quay ve 0

        // Release FIFO0
        CAN1->RF0R |= CAN_RF0R_RFOM0;
    }
}


/* ================== MAIN ================== */

int main(void)
{
    UART1_Init(115200);
    UART1_TxStr("\r\n[BOOT] OK\r\n");
 
    
    CAN_GPIO_Init();

    CAN_Setup();
    UART1_TxStr("[CAN] Loopback Enabled\r\n");

    uint8_t txdata[8] = {0x11,0x22,0x33,0x44,0xAA,0xBB,0xCC,0xDD};
		
		UART1_TxStr("[CAN] Sending frame...\r\n");
    CAN_Send(0x321, txdata, 8); 

    while (1)
    {
        
        while (can_rx_tail != can_rx_head)
        {
           volatile CAN_Frame_t *frame = &can_rx_ring[can_rx_tail];

            // In ID
            UART1_TxStr("[CAN] RX ID=");
            if (frame->ide)
            {
                // Extended ID 29-bit
                UART1_TxStr("0x");
                UART1_TxHex16((frame->id >> 16) & 0x1FFF); // 13-bit cao
                UART1_TxHex16(frame->id & 0xFFFF);         // 16-bit thap
            }
            else
            {
            
                UART1_TxStr("0x");
                UART1_TxHex16(frame->id & 0x7FF);
            }
            UART1_TxStr(" DATA=");
            for (uint8_t i = 0; i < frame->len; i++)
            {
                UART1_TxHex8(frame->data[i]);
                UART1_TxChar(' ');
            }
            UART1_TxStr("\r\n");

          
            can_rx_tail = (can_rx_tail + 1) % CAN_RX_RING_SIZE;
        }

       
    }

}
