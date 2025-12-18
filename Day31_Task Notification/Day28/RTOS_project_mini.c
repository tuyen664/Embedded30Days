/*
  @brief   : FreeRTOS UART Command + Timer Monitor Demo (TX+RX Interrupt)
  @details : Register-level, No HAL – UART RX/TX via Interrupt
*/

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"
#include <string.h>

/* ================== DEFINES ================== */
#define LED_PIN       13
#define LED_ON()      (GPIOC->BRR = (1 << LED_PIN))
#define LED_OFF()     (GPIOC->BSRR = (1 << LED_PIN))
#define LED_TOGGLE()  (GPIOC->ODR ^= (1 << LED_PIN))

#define CMD_LED_ON      1
#define CMD_LED_OFF     2
#define CMD_LED_TOGGLE  3
#define CMD_UNKNOWN     0xFF

#define UART_BAUD     9600
#define CMD_BUF_LEN   32

/* ================== PROTOTYPES ================== */
static void SystemClock_Config(void);
static void GPIO_Config(void);
static void UART1_Config(uint32_t baud);
static void UART1_SendChar_IT(char c);
static void UART1_SendString_IT(const char *s);
static void Task_UART_Receiver(void *pv);
static void Task_LED_Control(void *pv);
static void vTimerCallback_Monitor(TimerHandle_t xTimer);

static void Task_UART_TX(void *pv);


/* ================== GLOBALS ================== */
//QueueHandle_t xQueueCmd;
QueueHandle_t xQueueTxMsg;
TaskHandle_t xLedTaskHandle = NULL;
QueueHandle_t xQueueRxChar;
QueueHandle_t xQueueTxChar;
TimerHandle_t xTimerMonitor;
SemaphoreHandle_t xUartMutex;



/* ================== MAIN ================== */
int main(void)
{
    SystemClock_Config();
    GPIO_Config();
    UART1_Config(UART_BAUD);

    /* Create RTOS objects */
   // xQueueCmd     = xQueueCreate(5, CMD_BUF_LEN);
	  xQueueTxMsg = xQueueCreate(8, sizeof(char *));

    xQueueRxChar  = xQueueCreate(32, sizeof(char));
    xQueueTxChar  = xQueueCreate(64, sizeof(char));
    xUartMutex    = xSemaphoreCreateMutex();
    xTimerMonitor = xTimerCreate("Monitor", pdMS_TO_TICKS(1000), pdTRUE, 0, vTimerCallback_Monitor);

    
    /* Test message */
    UART1_SendString_IT("\r\n[SYS] UART TX+RX Interrupt Demo\r\n");
    UART1_SendString_IT("Commands: ON / OFF / TOGGLE\r\n");

    /* Create Tasks */
	
	  xTaskCreate(Task_UART_TX, "UART_TX", 256, NULL, 4, NULL);

    xTaskCreate(Task_UART_Receiver, "UART_RX", 256, NULL, 3, NULL);
    xTaskCreate(Task_LED_Control,  "LED_CTRL", 256, NULL, 2, &xLedTaskHandle);

		
		if (xLedTaskHandle == NULL || xQueueRxChar == NULL || xQueueTxChar == NULL ||
        xUartMutex == NULL || xTimerMonitor == NULL)
    {
        while (1);
    }
		
    xTimerStart(xTimerMonitor, 0);
    vTaskStartScheduler();

    while (1);
}

/* ================== CLOCK CONFIG ================== */
static void SystemClock_Config(void)
{
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;
    RCC->CFGR |= RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

/* ================== GPIO CONFIG ================== */
static void GPIO_Config(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;
    GPIOC->CRH &= ~(0xF << 20);
    GPIOC->CRH |=  (0x3 << 20);  // PC13 push-pull output
    LED_OFF();
}

/* ================== UART CONFIG ================== */
static void UART1_Config(uint32_t baud)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_IOPAEN;

    // TX: PA9 (AF Push-Pull)
    GPIOA->CRH &= ~(0xF << 4);
    GPIOA->CRH |=  (0xB << 4);

    // RX: PA10 (Input floating)
    GPIOA->CRH &= ~(0xF << 8);
    GPIOA->CRH |=  (0x4 << 8);

    uint32_t pclk = 72000000;
    USART1->BRR = pclk / baud;

    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;
    USART1->CR1 &= ~USART_CR1_TXEIE; // Disable TX interrupt initially

    NVIC_SetPriority(USART1_IRQn, 8);
    NVIC_EnableIRQ(USART1_IRQn);
}

/* ================== UART TX (INTERRUPT-BASED) ================== */
void UART1_SendChar_IT(char c)
{
    if (xQueueTxChar == NULL) return;

    taskENTER_CRITICAL();

    //  ghi ky tu dau tien ngay
    if ((USART1->SR & USART_SR_TXE) && !(USART1->CR1 & USART_CR1_TXEIE))
    {
        USART1->DR = c; // ngat TXE dc kich hoat khi 0 -> 1
        USART1->CR1 |= USART_CR1_TXEIE;  // Enable TX interrupt
    }
    else
    {
        // dua vao hang doi neu dang ban
        xQueueSend(xQueueTxChar, &c, 0);
        USART1->CR1 |= USART_CR1_TXEIE;
    }

    taskEXIT_CRITICAL();
}

void UART1_SendString_IT(const char *s)
{
    while (*s)
        UART1_SendChar_IT(*s++);
}

static void Task_UART_TX(void *pv)
{
	 (void)pv;
    const char *msg;

    for (;;)
    {
        if (xQueueReceive(xQueueTxMsg, &msg, portMAX_DELAY) == pdPASS)
        {
            UART1_SendString_IT(msg);
        }
    }
}




/* ================== UART ISR ================== */
void USART1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // --- RX interrupt ---
    if (USART1->SR & USART_SR_RXNE) // RXNE =1 la kich hoat ngat
    {
        char c = (USART1->DR & 0xFF); //  RXNE =0
        if (c >= 'a' && c <= 'z') c -= 32;  // to uppercase
        xQueueSendFromISR(xQueueRxChar, &c, &xHigherPriorityTaskWoken);
    }

    // --- TX interrupt ---
    if ((USART1->SR & USART_SR_TXE) && (USART1->CR1 & USART_CR1_TXEIE))
    {
        char txChar;
        if (xQueueReceiveFromISR(xQueueTxChar, &txChar, &xHigherPriorityTaskWoken) == pdPASS)
        {
            USART1->DR = txChar; // Send next char
        }
        else
        {
            USART1->CR1 &= ~USART_CR1_TXEIE; // No more data disable interrupt
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ================== TASK: UART Receiver ================== */
static void Task_UART_Receiver(void *pv)
{
    (void) pv;
    char buffer[CMD_BUF_LEN];
    uint8_t idx = 0;
    char c;

    for (;;)
    {
        if (xQueueReceive(xQueueRxChar, &c, portMAX_DELAY) == pdPASS)
        {
           UART1_SendChar_IT(c); // Echo back

            if (idx == 0 && c != '\r' && c != '\n')
            {
                xTimerStop(xTimerMonitor, 0);
            }

            if (c == '\r' || c == '\n')
            {
                buffer[idx] = '\0';
                if (idx > 0)
                {
									 uint32_t cmd;

                   if (strcmp(buffer, "ON") == 0)
                       cmd = CMD_LED_ON;
                   else if (strcmp(buffer, "OFF") == 0)
                       cmd = CMD_LED_OFF;
                   else if (strcmp(buffer, "TOGGLE") == 0)
                       cmd = CMD_LED_TOGGLE;
                   else
                       cmd = CMD_UNKNOWN;

              xTaskNotify(xLedTaskHandle, cmd, eSetValueWithOverwrite);// chi can lenh moi nhat

              idx = 0;
              xTimerStart(xTimerMonitor, 0);
                }
            }
            else if (idx < CMD_BUF_LEN - 1)
            {
                buffer[idx++] = c;
            }
        }
    }
}

/* ================== TASK: LED Control ================== */
static void Task_LED_Control(void *pv)
{
    (void) pv;
    uint32_t cmd;
    const char *msg;

    for (;;)
    {
        xTaskNotifyWait(
            0x00,
            0xFFFFFFFF,
            &cmd,
            portMAX_DELAY
        );

        switch (cmd)
        {
        case CMD_LED_ON:
            LED_ON();
            msg = "[LED] Turned ON\r\n";
            break;

        case CMD_LED_OFF:
            LED_OFF();
            msg = "[LED] Turned OFF\r\n";
            break;

        case CMD_LED_TOGGLE:
            LED_TOGGLE();
            msg = "[LED] Toggled\r\n";
            break;

        default:
            msg = "[ERR] Unknown command\r\n";
            break;
        }

        xQueueSend(xQueueTxMsg, &msg, portMAX_DELAY);
    }
}


/* ================== TIMER CALLBACK ================== */
static void vTimerCallback_Monitor(TimerHandle_t xTimer)
{
    (void)xTimer;
    const char *msg = "[SYS] System alive...\r\n";
    xQueueSend(xQueueTxMsg, &msg, 0);
}

