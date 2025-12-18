## So sánh Task Notification và EventGroup?

- Nếu chỉ cần signal giữa ISR và task thì dùng Task Notification vì nhanh hơn, ít RAM hơn

- EventGroup dùng khi nhiều task cần chờ cùng event

```c
static TaskHandle_t xEventTaskHandle = NULL;
```

```c
xTaskCreate(Task_EventHandler,
            "Event",
            256,
            NULL,
            2,
            &xEventTaskHandle);
```

 **ISR TIMER → Notification**

```c
xTaskNotifyFromISR(
    xEventTaskHandle,
    EVENT_BIT_TIMER,
    eSetBits,
    &xHigherPriorityTaskWoken
);
```

  **ISR TIMER → Notification**
 
```c
xTaskNotifyFromISR(
    xEventTaskHandle,
    EVENT_BIT_BUTTON,
    eSetBits,
    &xHigherPriorityTaskWoken
);
```

  **TASK EVENT HANDLER**

```c
static void Task_EventHandler(void *pvParameters)
{
    (void)pvParameters;
    uint32_t ulNotifyValue;

    for (;;)
    {
        xTaskNotifyWait(
            0x00,                          // Không clear trước
            EVENT_BIT_TIMER | EVENT_BIT_BUTTON,
            &ulNotifyValue,
            portMAX_DELAY
        );

        if (ulNotifyValue & EVENT_BIT_TIMER)
        {
            if (xSemaphoreTake(xUART_Mutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                UART1_SendString("[EVT] Timer tick\r\n");
                xSemaphoreGive(xUART_Mutex);
            }
        }

        if (ulNotifyValue & EVENT_BIT_BUTTON)
        {
            LED_TOGGLE();
            if (xSemaphoreTake(xUART_Mutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                UART1_SendString("[EVT] Button pressed\r\n");
                xSemaphoreGive(xUART_Mutex);
            }
        }
    }
}
```

## Giải thích

```c
xTaskNotifyWait(
            0x00,                          // Không clear trước
            EVENT_BIT_TIMER | EVENT_BIT_BUTTON,
            &ulNotifyValue,
            portMAX_DELAY
        );
```

- Hàm để chờ NOtification từ task khác hoặc ISR 

  Task sẽ block , chỉ được đánh thức khi : Có notification gửi tới hoặc có timeout

  Mỗi task chỉ có duy nhất 1 biến notification 32-bit (ulNotifyvalue)

- Tại sao dùng 0x00 ở đây , tại sao không clear trước 

  **Vì ISR có thể gửi notification TRƯỚC khi task kịp chạy**

   Nếu ta clear trước xảy ra lỗi mất event 

   99% code dùng 0x00

- EVENT_BIT_TIMER | EVENT_BIT_BUTTON : sau khi task nhận notification , clear các bit này
```c
EVENT_BIT_TIMER = (1 << 0)
EVENT_BIT_BUTTON = (1 << 1)
```
  Notification KHÔNG tự clear toàn bộ, chỉ clear theo tham số 2 (clear sau khi đã xử lý xong ở ulNotifyValue)

- &ulNotifyValue : địa chỉ lưu giá trị uint32_t : ulNotifyValue = 0b00000011
```c
BaseType_t xTaskNotifyWait(
    uint32_t ulBitsToClearOnEntry,
    uint32_t ulBitsToClearOnExit,
    uint32_t *pulNotificationValue,
    TickType_t xTicksToWait
);
```
- ulBitsToClearOnEntry : clear bit trước khi Task bắt đầu chờ
- ulBitsToClearOnExit : Clear sau khi Task đã nhận notification








