      ┌────────────────────┐
      │     TIM2 IRQ       │   (Interrupt xảy ra)
      │  (ISR Context)     │
      └─────────┬──────────┘
                │
                │ **vTaskNotifyGiveFromISR(xTimerTaskHandle)**
                │
                ▼
┌─────────────────────────────────────┐
│        Task Control Block (TCB)     |
│  (xTimerTaskHandle trỏ vào đây)     │
│                                     │
│  • Task State        : BLOCKED      │
│  • Priority          : 2            │
│  • Stack Pointer    : 0x20001ABC    │
│  • NotificationVal  : 0 → 1         │ ──> **ISR tăng giá trị**
│                                     │
└──────────────────────────────────── ┘
                │
                │ Scheduler check (Có task ưu tiên cao hơn?)
                │ 
                ▼
  ┌──────────────────────────────┐
  │    Task_TimerHandler Task    │
  │                              │
  │ ulTaskNotifyTake() -> UNBLOCK   
  │ LED_TOGGLE()                 │
  │ UART print                   │
  └──────────────────────────────┘

