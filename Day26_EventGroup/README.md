## Tổng Quan

**1. Tính năng**
- Sử dụng EventGroup để đồng bộ giữa ISR và task
- Sử dụng Mutex để bảo vệ UART (khi nhiều task gửi dữ liệu)
- Đọc ADC liên tục, gửi giá trị qua UART
- Nháy LED khi nút nhấn
- Ghi log qua UART khi Timer tick hoặc nút nhấn
  
**2. Peripherals**
- GPIO: PC13 LED, PA1 nút nhấn, PA0 ADC
- UART1: debug/log
- Timer2: tick 1Hz
- EXTI1: ngắt nút nhấn
  
**3. RTOS elements**
- Tasks: Task_ADC, Task_Status, Task_EventHandler
- Mutex: xUART_Mutex (bảo vệ UART)
- EventGroup: xEventGroup (đánh dấu sự kiện từ ISR)

## Chi tiết

**1. GPIO, UART, ADC, Timer, EXTI**

**1.1 GPIO_Config()**

- PC13: output push-pull cho LED
- PA1: input pull-up cho nút nhấn (EXTI1)
  
**1.2 UART1_Config()**

- PA9 TX, PA10 RX
- Baudrate 9600
- Bảo vệ UART khi nhiều task dùng → cần Mutex
  
**1.3 ADC1_Config()**
- cấu hình ADC, sampling PA0
- readADC_Avg() đọc trung bình nhiều sample → giảm nhiễu
  
**1.4 Timer2 và EXTI1**
- Timer2: tạo tick 1Hz, ISR đặt EVENT_BIT_TIMER
- EXTI1: nút nhấn, ISR đặt EVENT_BIT_BUTTON
- Cả hai ISR dùng **xEventGroupSetBitsFromISR() + portYIELD_FROM_ISR()** → ưu tiên task ngay khi cần

**2. EventGroup và Task_EventHandler**

**2.1 xEventGroupWaitBits()**

- Chờ any bit (OR) của **EVENT_BIT_TIMER hoặc EVENT_BIT_BUTTON**
- pdTRUE → tự động xóa bit sau khi nhận
- portMAX_DELAY → block vô thời hạn đến khi bit được set
  
**2.2 Logic trong task**
```c
if (uxBits & EVENT_BIT_TIMER) // gửi log "[EVT] Timer tick"
if (uxBits & EVENT_BIT_BUTTON) // toggle LED + log "[EVT] Button pressed"
```

**2.3 Task_ADC và Task_Status**
- Task_ADC: đọc ADC trung bình 8 sample, gửi UART, delay 1s
- Task_Status: gửi "[STATUS] System OK" mỗi giây, bảo vệ UART bằng Mutex
  
**Lưu ý :**

- Trong project thực tế: Nhiều task log cùng UART → cần timeout khi lấy Mutex hoặc dùng queue thay vì trực tiếp gửi UART
- Không dùng portMAX_DELAY trừ khi chắc chắn task release nhanh , Nên dùng timeout hợp lý
- Cân nhắc priority :
  
   Task cần log quan trọng → priority cao
  
   Task không quan trọng → priority thấp
  
- **Timeout có lợi hơn portMAX_DELAY:**
- Task không block vô hạn
- Giảm nguy cơ starvation
- Hệ thống nhiều task ổn định hơn
```c
for (;;) 
{
    if (xSemaphoreTake(xUART_Mutex, pdMS_TO_TICKS(50)) == pdTRUE)
 {
        UART1_SendString("ADC Value");
        xSemaphoreGive(xUART_Mutex);
 } 
   else {
        // không lấy được mutex → làm việc khác, ví dụ tính toán, cập nhật biến
        
        }
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

**2.4 ISR + EventGroup**
- Không làm việc nặng trong ISR
- Chỉ set bit event
- Dùng portYIELD_FROM_ISR() → task high priority chạy ngay
- **Không gọi trực tiếp UART trong ISR**

**3. Lưu ý quan trọng**
- Không bao giờ lấy Mutex trong ISR → deadlock
- Khi dùng xEventGroupWaitBits() : pdTRUE sẽ clear bit → nếu bit set nhiều lần trước khi task chạy, chỉ nhận 1 lần
  
   pdFALSE → bit vẫn giữ → task sẽ nhận nhiều lần
  
- Trong readADC_Avg() có vTaskDelay(pdMS_TO_TICKS(2))
  
   Không dùng delay_ms() (busy wait) trong task → block CPU
  
- Task_ADC: 384 → vì snprintf buffer + task call stack -> Nếu stack nhỏ → crash trong runtime

- uxBits trả về một giá trị kiểu EventBits_t, chứa tập hợp các bit đã được set tại thời điểm hàm được thỏa mãn
- uxBits là một giá trị 8/16/32-bit (tùy config), thường là uint32_t, chứa:
  
    1 << 0 nếu Timer event
  
    1 << 1 nếu Button event
  
    hoặc cả hai: 0b00000011

## Video

https://drive.google.com/file/d/1Ufo8KUIpQyb1aPNtdvYZM3mus0Xezzh8/view?usp=drive_link

