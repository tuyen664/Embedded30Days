## Tổng Quan
- Task_ADC_Read: đọc ADC từ PA0, trung bình 5 lần, gửi dữ liệu vào queue (xQueueADC)
- Task_PWM_Control: nhận dữ liệu từ queue, tính duty cycle, điều khiển PWM trên PB9, đồng thời in giá trị duty qua UART với mutex bảo vệ
- Task_UART_Command: định kỳ in trạng thái “ADC active” qua UART.
- Timer Monitor: nhấp nháy LED PC13 và gửi message [SYS] Alive mỗi 3 giây

## Giải thích các phần quan trọng 
**1. Queue (xQueueADC)**
```c
xQueueADC = xQueueCreate(10, sizeof(uint16_t));
```
- Tạo queue chứa tối đa 10 giá trị uint16_t
- Task đọc ADC :  xQueueSend(), task xử lý PWM  : xQueueReceive()

**2. Mutex cho UART (xUART_Sem)**
- xSemaphoreTake(xUART_Sem, portMAX_DELAY) → bảo vệ UART, tránh race condition khi nhiều task cùng in
-  Mutex cũng hỗ trợ **priority inheritance** tự động → tránh priority inversion
  
**3. Timer Monitor**
```c 
xTimerCreate("Monitor", pdMS_TO_TICKS(3000), pdTRUE, 0, vTimerCallback_Monitor)
```
- Timer chạy cứ 3 giây → gọi callback toggle LED + in UART
  
**4. Task_ADC_Read**
```c
adc_send = readADC_Avg (ADC_CHANNEL,5);
xQueueSend(xQueueADC, &adc_send, 0);
vTaskDelay(pdMS_TO_TICKS(200));
```
- Đọc ADC và gửi vào xQueueADC cho task khác sử lý

**5.Task_PWM_Control**
```c
if (xQueueReceive(xQueueADC, &adc_recv, portMAX_DELAY) == pdPASS)
{
    uint16_t duty =(adc_recv * 1000 / 4095);
    TIM4->CCR4 = duty;
    TIM4->EGR |= 1;  // update PWM
}
```
- Nhận ADC từ xQueueADC , chuyển sang duty PWM 0–1000
- Mutex UART để in duty value an toàn

## Ảnh chụp 

