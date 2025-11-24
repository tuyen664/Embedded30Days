# FreeRTOS Software Timer

## Tổng Quan
- FreeRTOS Software Timer là timer chạy trong Task Timer Daemon của FreeRTOS, không chạy trong ISR, không dùng hardware timer của MCU.
- Chạy trong timer service task (priority mặc định = configTIMER_TASK_PRIORITY)
- Chạy không blocking và không nên làm việc nặng (vì tất cả timer share chung 1 task)
- Thời gian kích hoạt dựa trên tick interrupt → độ chính xác phụ thuộc tick rate


## Tạo 3 timer software
```c
xTimer_LED  = xTimerCreate("LED_Timer",  pdMS_TO_TICKS(1000), pdTRUE,  0, vTimerCallback_LED);
xTimer_UART = xTimerCreate("UART_Timer", pdMS_TO_TICKS(1000), pdTRUE,  0, vTimerCallback_UART);
xTimer_UART2 = xTimerCreate("UART2_Timer", pdMS_TO_TICKS(1000), pdTRUE,  0, vTimerCallback_UART2);
```
- pdTRUE : Auto-reload → lặp liên tục
- 0 : ID (parameter) của timer → ta đang để NULL
- Callback : Hàm sẽ được gọi khi timer hết hạn
- pdMS_TO_TICKS(1000) : Sau mỗi 1s gọi hàm **vTimerCallback** 
- "LED_Timer" : Tên timer, dùng để debug

- Start timer 
```c
xTimerStart(xTimer_LED, 0);
xTimerStart(xTimer_UART, 0);
xTimerStart(xTimer_UART2, 0);
```
- block_time = 0 → không chờ queue khi gửi yêu cầu kích hoạt timer
- Nếu queue timer đầy (hiếm), lệnh start có thể thất bại-> ở đây ta chưa check điều đó

**1. vTimerCallback_LED()**
```c
static void vTimerCallback_LED(TimerHandle_t xTimer)
{
    (void)xTimer;  // không dùng ID
    LED_TOGGLE();
}
```
- Mỗi 1 giây → đảo trạng thái LED.
  
**2. vTimerCallback_UART()**
```c
static void vTimerCallback_UART(TimerHandle_t xTimer)
{
    UART1_SendString("[UART] Timer event: system alive!\r\n");
}
```
- Mỗi 1 giây → in một dòng UART
  
**3. vTimerCallback_UART2()**
```c
static void vTimerCallback_UART2(TimerHandle_t xTimer)
{
    UART1_SendString("[TEST] YESSSSSSSSSSSSS!\r\n");
}
```
- Cứ mỗi giây → in thêm một chuỗi

-> Như vậy mỗi giây UART1 in 2 dòng

## Các lưu ý quan trọng

**1. Software Timer KHÔNG chạy trong interrupt**
- Software timer của FreeRTOS chạy KHÔNG phải trong ISR
- Nó chạy trong Timer Service Task, chính là 1 task bình thường
  
**2. Tất cả timer share cùng 1 task!**
- Ta có 3 timer → nhưng thực tế chỉ 1 task duy nhất xử lý chúng
- Nếu 1 callback chạy lâu → 2 callback còn lại bị delay
- Vì vậy : Không được gửi message lớn , Không được xử lý tính toán nặng ,Không được delay (vTaskDelay)
- Callback phải ngắn và gọn.

**3. in UART trong callback = KHÔNG AN TOÀN nếu tốc độ baud thấp**
- UART ở 9600 baud → tốc độ gửi rất chậm
- Khi in mỗi dòng khoảng 30 ký tự → khoảng 3ms -> in 2 dòng → mất gần 6ms
- Nếu tick = 1ms → có nghĩa là timer service task đang bận 6 tick liên tục
- Kéo theo Timer callback khác bị delay
- Task khác thấp hơn priority có thể bị block
  
**4. Timer callback KHÔNG bao giờ được block**
- không được vTaskDelay() , xSemaphoreTake(block) , xQueueReceive(block) , hay bất kì hàm block nào
- Vì ta sẽ block luôn **timer service task**, làm toàn bộ timer chết theo

**5. Ta đang dùng parameter = 0 (NULL)**
- xTimerCreate(... , 0 , ...); → nghĩa là ta không truyền ID cho timer.
- Trong project thật : Timer thường được dùng để quản lý nhiều object → ID cực kỳ quan trọng
  
**6. UART gửi trong callback = không tốt**
- Cách đúng :  Callback chỉ gửi 1 message vào queue UART -> Task UART gửi thật
  
**7. Nguyên tắc: 1 peripheral = 1 task xử lý**
- Ta đang viết thẳng vào USART1->DR trong callback timer → đây là race-condition nếu có nhiều task cùng gửi UART.
- cách chuẩn : Một task UART duy nhất -> gộp cả 2 uart vào 1
  
**8. Timer sẽ bị trễ**
- Software Timer không có độ chính xác tuyệt đối
- Chỉ đảm bảo: “thực thi càng sớm càng tốt khi timer task được chạy”
  
**9. Timer callback chạy với priority của Timer Task**
- Priority = configTIMER_TASK_PRIORITY
- Nếu ta để priority rất cao → callback sẽ giật CPU của các task khác.

## Các đoạn code bổ xung 

**Timer callback – gửi event vào Queue**

```c
/* Event type */
typedef enum {
    EVT_TIMER_UART,
} Event_t;

/* Queue handle */
extern QueueHandle_t xEventQueue;

/* Timer callback */
void vTimerCallback_UART(TimerHandle_t xTimer)
{
    Event_t evt = EVT_TIMER_UART;

    /* Gửi event vào queue, không block */
    xQueueSendFromISR(xEventQueue, &evt, NULL);
}
```

**Task – nhận event và xử lý**

```c
void vTaskEventProcess(void *pvParameters)
{
    Event_t evt;

    while (1)
    {
        /* Chờ event (block vô thời hạn) */
        if (xQueueReceive(xEventQueue, &evt, portMAX_DELAY) == pdTRUE)
        {
            switch (evt)
            {
                case EVT_TIMER_UART:
                    UART1_SendString("[UART] Timer event processed in task!\r\n");
                    break;
            }
        }
    }
}
```
- lưu ý : EVT_TIMER_UART và Event_t là ta tự đặt tên

  **Kiểm tra Start Timer**
  
```c
BaseType_t status1, status2, status3;

status1 = xTimerStart(xTimer_LED, 0);
status2 = xTimerStart(xTimer_UART, 0);
status3 = xTimerStart(xTimer_UART2, 0);

if (status1 == pdPASS && status2 == pdPASS && status3 == pdPASS)
{
    UART1_SendString("[OK] All timers started successfully\r\n");
}
else
{
    UART1_SendString("[ERR] One or more timers FAILED to start!\r\n");

    if (status1 != pdPASS) UART1_SendString(" -> LED Timer failed\r\n");
    if (status2 != pdPASS) UART1_SendString(" -> UART Timer failed\r\n");
    if (status3 != pdPASS) UART1_SendString(" -> UART2 Timer failed\r\n");

    while (1); // stop system
}
```


## Ảnh chụp 

