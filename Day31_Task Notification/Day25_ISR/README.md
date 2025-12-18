```
      ┌────────────────────┐
      │     TIM2 IRQ       │   (Interrupt xảy ra)
      │  (ISR Context)     │
      └─────────┬──────────┘
                │
                │ vTaskNotifyGiveFromISR(xTimerTaskHandle)
                │
                ▼
┌─────────────────────────────────────┐
│        Task Control Block (TCB)     |
│  (xTimerTaskHandle trỏ vào đây)     │
│                                     │
│  • Task State        : BLOCKED      │
│  • Priority          : 2            │
│  • Stack Pointer    : 0x20001ABC    │
│  • NotificationVal  : 0 → 1         │ ──> ISR tăng giá trị
│  • Waiting reason : Notify          │
└──────────────────────────────────── ┘
                │
                │ Scheduler check (Có task ưu tiên cao hơn?)
                │ 
                ▼
  ┌──────────────────────────────┐
  │    Task_TimerHandler Task    │
  │                              │
  │ ulTaskNotifyTake() -> UNBLOCK|   
  │ LED_TOGGLE()                 │
  │ UART print                   │
  └──────────────────────────────┘
```

## ISR làm gì ?
```c
vTaskNotifyGiveFromISR(xTimerTaskHandle, &xHigherPriorityTaskWoken);
```
- ISR làm 3 việc sau :

  + Xác định Task nào cần báo 

  + Tăng **notification value** trong TCB

  + Báo scheduler nếu cần context switch (phát hiện task priority cao hơn vừa unblock)

## xTimerTaskHandle

```c
TaskHandle_t xTimerTaskHandle;
```
- TaskHandle_t là con trỏ tới TCB (Task Control Block)

```c
xTaskCreate(Task_TimerHandler, "Timer",  256, NULL, 2,  &xTimerTaskHandle);
```

- FreeRTOS tạo task , Sau đó ghi handle của task đó vào biến mà ta truyền vào
- &xTimerTaskHandle = địa chỉ của biến mà FreeRTOS ghi task handle

- Ta dùng **xTimerTaskHandle** để cho FreeRTOS biết phải notify cho task nào

- Task Notification là 1–1 (1 sender → 1 task) (semaphore nhiều task có thể take)
- Luôn lưu handle nếu task:

   Nhận ISR signal

   Nhận command từ task khác


## TCB chứa gì ?

- Task State : Ready/ Blocked/ Suspended
- Priority : Scheduler dùng để chọn Task
- Stask Pointer : Context switching
- Notification Value : Task Notification
- Waiting Reason : vì delay / semaphore / notify

 -> Notification nằm trong TCB , không cần cấp phát RAM riêng 

## Vì sao Notification nhanh hơn Semaphore 
- Semaphore : ISR -> Kernel Object -> Queue -> Task
- Notification : ISR -> TCB -> Task

  -> ít bước hơn , không malloc , ít lock scheduler

- Task Notification là signal nằm trực tiếp trong TCB, ISR chỉ cần biết TaskHandle để đánh thức task đó, không cần kernel object trung gian

## Khi nào dùng và không dùng NOtification 

- ISR → 1 task : ISR → 1 task
- ISR -> nhiều task : EventGroup
- Task -> Task (signal) : Notification
- Truyền data : Queue
- Bảo vệ UART / SPI : Mutex
- DMA done : Notification
- Button debounce : Notification

## Lưu ý 

- 1 task chỉ có 1 notification slot
- Task Notification dùng khi cần signal nhanh giữa ISR–Task hoặc Task–Task, chỉ cho 1 task, không truyền data phức tạp
- Không dùng khi cần truyền tin (broadcast) , truyền struct hoặc bảo vệ tài nguyên




















