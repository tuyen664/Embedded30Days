## Tổng Quan
- LED nháy bởi **TIM2 ISR → Binary Semaphore → Task**
- ADC đọc và in giá trị mỗi 1 giây
- Task Status in tình trạng hệ thống
- Dùng UART mutex khi nhiều task cùng in UART
- ISR sử dụng **xSemaphoreGiveFromISR + portYIELD_FROM_ISR**

## Các kiến thức quan trọng
**1. Vì sao xSemaphoreGiveFromISR() phải gọi trong ISR?**
- Vì FreeRTOS tách thành 2 nhóm API: Normal API (Task context) , FromISR API (Interrupt context)
- Nếu bạn gọi API normal trong ISR → crash hoặc lỗi bất thường
  
**2. Tại sao phải dùng portYIELD_FROM_ISR()?**
- Vì sau khi interrupt giải phóng semaphore, có thể có task priority cao hơn vừa được unblock → **cần chạy ngay**
- Ví dụ : Task_TimerHandler priority = 2 , Các task khác = 1
- ISR Give semaphore → Task_TimerHandler READY → priority cao nhất → cần chạy NGAY **không chờ tick*8
-> portYIELD_FROM_ISR() ép FreeRTOS context switch ngay trong ISR

**3. LỖI QUAN TRỌNG: Priority của TIM2 phải ≥ 8**
```C
NVIC_SetPriority(TIM2_IRQn, 8);
```
- Điều này cực kì quan trọng vì :
```c
configMAX_SYSCALL_INTERRUPT_PRIORITY = 128 (tuong duong NVIC prio 8)
```
- FreeRTOS quy định : Interrupt nào muốn gọi API “FromISR” phải có priority số lớn hơn (trong Cortex-M: số lớn = ưu tiên thấp)
- ISR muốn gọi API FromISR → priority phải ≤ ưu tiên thấp, tức số priority ≥ configMAX_SYSCALL_INTERRUPT_PRIORITY
  
**4. Vì sao Mutex không phải Binary Semaphore?**
- Mutex có: Priority inheritance , “Owner” (chỉ 1 task dùng 1 thời điểm )
- Binary semaphore KHÔNG có 2 tính chất này

**5. Tại sao trong Timer ISR không toggle LED trực tiếp?**
- ISR phải **siêu nhanh**
- Toggle LED + UART sẽ nặng → làm trễ các interrupt khác
- FreeRTOS bị ảnh hưởng nếu ISR chạy dài
-> Thiết kế chuẩn: “ISR chỉ báo sự kiện”

## Giải thích chi tiết các Task
**1. Task_TimerHandler – Priority = 2**
- Đợi semaphore (binary) từ ISR
- Khi nhận được: LED_TOGGLE() , Gửi UART message
- Vì priority = 2 → nó là task quan trọng nhất trong hệ thống.

**2. Task_Status – Priority = 1**
- Cứ mỗi 1s:  xin UART mutex , in dòng “System OK” , Nếu mutex bận → đợi tối đa 100ms
- Nếu semaphore không được trả trước 100 ms, timeout xảy ra. FreeRTOS chuyển task từ **Blocked → Ready**
- Scheduler phải duyệt priority và chọn trong các task đã READY.
  
**3. Task_ADC – Priority = 1**
- Cứ mỗi 1s: đọc ADC 8 lần , average , gửi UART (qua mutex), ADC đọc delay 2ms mỗi sample → tổng 16ms

**4. TIM2 ISR**
```c
void TIM2_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
```
- Biến này giúp FreeRTOS biết có cần “context switch” sau khi ISR kết thúc hay không
```c
if (TIM2->SR & TIM_SR_UIF)
{
    TIM2->SR &= ~TIM_SR_UIF;  
```
- Phải xóa cờ trước khi thoát, nếu không → ISR gọi liên tục

```c
xSemaphoreGiveFromISR(xTimerSemaphore, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```
- Cho semaphore , Nếu task TimerHandler priority cao hơn task hiện tại → yield ngay

**4.1 Cách hoạt động tổng thể**
- ISR gọi xSemaphoreGiveFromISR()
- API kiểm tra: có task unblock nào priority cao hơn đang chạy không
- Nếu có → gán *pxHigherPriorityTaskWoken = pdTRUE
- Sau đó ta gọi portYIELD_FROM_ISR(xHigherPriorityTaskWoken)
- Scheduler sẽ preempt task hiện tại, chạy task high priority ngay

## Các lưu ý 
- delay_ms() kiểu for-loop không ổn -> có thể thay bằng SysTick

- xTimerSemaphore là “cơ chế báo sự kiện” , còn xHigherPriorityTaskWoken là “cờ báo Scheduler có cần chạy task unblock ngay không”
- xTimerSemaphore thay đổi khi **ISR give** -> Task_TimerHandler unblock, semaphore reset
- xHigherPriorityTaskWoken thay đổi khi **ISR give, task unblock, priority cao hơn** -> portYIELD_FROM_ISR → preempt task hiện tại

- Task_TimerHandler không có vTaskDelay() 
- Task luôn chờ semaphore với timeout = portMAX_DELAY, tức là block vô hạn cho đến khi ISR signal.
- Task_TimerHandler hoàn toàn phụ thuộc vào ISR để “đánh thức” nó
-** Mẹo thực tế**
- Các task cần phản ứng thời gian thực với sự kiện ngoại vi → không dùng vTaskDelay, dùng semaphore/queue/event group từ ISR
- Các task lặp định kỳ không cần chính xác cao → dùng vTaskDelay

## Video

https://drive.google.com/file/d/1nygW7SdgM8CUbmVBJte24bGbpWGK7aih/view?usp=drive_link

