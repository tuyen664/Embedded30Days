## Tổng Quan
- Đoạn code này mô phỏng một hệ thống RTOS có nhiều task cùng truy cập UART.
- Ta dùng Mutex để bảo vệ UART : Task đọc Sensor , Task báo Status , Task Log , Task đọc ADC và in ra UART
## Các phần quan trọng 
**1. Tạo Mutex**
```c
xUART_Mutex = xSemaphoreCreateMutex();
```
- Đây là Mutex chuẩn FreeRTOS, có **priority inheritance**
- Nếu không tạo được → báo lỗi và đứng luôn:
```c
if (xUART_Mutex == NULL) {
    UART1_SendString("[ERR] Mutex creation failed!\r\n");
    while (1);
}
```
- Nếu ta dùng **Binary Semaphore** thay cho Mutex → KHÔNG có **priority inheritance** → dễ gây priority inversion.
- **Mutex KHÔNG dùng trong ISR** 

**2. Tạo task**
```c
xTaskCreate(Task_Sensor, "Sensor", 256, NULL, 1, NULL);
xTaskCreate(Task_Status, "Status", 256, NULL, 1, NULL);
xTaskCreate(Task_Log,    "Log",    256, NULL, 1, NULL);
xTaskCreate(Task_ADC,    "ADC",    384, NULL, 1, NULL);
```
- Tất cả task đều có priority = 1 → cùng cấp → scheduler sẽ round-robin giữa chúng (nếu cùng trạng thái READY).
- Priority bằng nhau → công bằng nhưng nếu một task delay ít hơn → nó được chạy thường xuyên hơn
- pvParameters = NULL → nghĩa là task không nhận tham số bên ngoài



## Giải thích 4 task
**1. Task_Sensor**
```c
if (xSemaphoreTake(xUART_Mutex, pdMS_TO_TICKS(100)) == pdTRUE)
{
    UART1_SendString("[SENSOR] Reading data...\r\n");
    xSemaphoreGive(xUART_Mutex);
}
vTaskDelay(800);
```
- Thử lấy mutex trong 100ms , in ra chuỗi , trả mutex, delay 800ms
- Nếu Task_ADC giữ mutex quá lâu → Sensor có thể bị timeout → không in ra UART
  
**2. Task_Status**
```c
static void Task_Status(void *pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        if (xSemaphoreTake(xUART_Mutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            UART1_SendString("[STATUS] System OK\r\n");
            LED_TOGGLE();
            xSemaphoreGive(xUART_Mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```
- Tương tự task Sensor : In ra trạng thái hệ thống , Toggle Led , Delay 1s

**3. Task_Log**
- Logging mỗi 1.5 giây 
- Logging không nên cùng priority với task quan trọng
→ Trong project thật, Logging nên priority thấp nhất.

**4. Task_ADC**
```c
static void Task_ADC(void *pvParameters)
{
    (void)pvParameters;
    uint16_t adcValue;
    char buffer[32];

    for (;;)
    {
        adcValue = readADC_Avg(0, 8); // doc trung binh kenh PA0
        if (xSemaphoreTake(xUART_Mutex, portMAX_DELAY) == pdTRUE)
        {
            snprintf(buffer,sizeof(buffer), "[ADC] Value = %u\r\n", adcValue);
            UART1_SendString(buffer);
            xSemaphoreGive(xUART_Mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```
- Task ADC dùng xSemaphoreTake(xUART_Mutex, portMAX_DELAY) → nghĩa là chờ hoài đến khi có mutex, KHÔNG timeout.
=> Vì vậy task ADC KHÔNG BAO GIỜ bị timeout → không bị mất dữ liệu in ra UART
- readADC_Avg(): delay 2ms trong loop → Không gây block system → an toàn.

## Các lưu ý quan trọng 
**1. ADC**
- Bật Continuous mode nhưng lại dùng SWSTART mỗi lần → vẫn OK nhưng hơi thừa.
- Ta có thể xóa dòng này : ADC1->CR2 |= (1U << 22); trong readADC();
- ADC chạy ở 12 MHz (PCLK2/6) → nằm trong giới hạn (0.6–14 MHz)
- Sample time dài = loại nhiễu tốt hơn nhưng tăng thời gian chuyển đổi → chỉ đúng trong bài test sensor chậm.
  
**2. Các lỗi tiềm ẩn**
- Không dùng DMA cho ADC : Task_ADC phải chờ từng sample → CPU tốn thời gian -> dùng DMA + task đọc buffer
- UART gửi từng byte → chậm -> Nên viết hàm gửi buffer non-blocking hoặc dùng interrupt TX
- Mutex không nên giữ quá lâu : Nếu ta đặt snprintf() quá nặng → có thể gây block các task khác.
  
**3. Quan trọng**
- Các delay trong init hardware: Clock config , PLL lock , Flash latency , Reset peripheral , ADC calibration , USART enable ổn định , Sensor init
→ **luôn phải dùng delay dạng busy-wait**, không được dùng FreeRTOS delay
- Vì lúc đó : RTOS chưa chạy , tick chưa hoạt động , Task scheduling chưa tồn tại.
- delay_ms(1); -> chính xác

**4.Mutex KHÔNG dùng trong ISR** 
- Khi mutex được một task giữ, và task có priority cao hơn bị block → task thấp sẽ được nâng priority (priority inheritance).
- ISR không phải là task, không có priority của scheduler → không thể tham gia cơ chế này
- Nếu ISR dùng mutex → FreeRTOS không thể xử lý priority inheritance → gây deadlock hoặc lỗi scheduler.
- Mutex là cơ chế BLOCKING → ISR không được phép block , ISR chạy ở chế độ interrupt, không chịu quản lý của scheduler
- ISR phải chạy **rất nhanh** và return ngay để CPU trở lại chạy task
- FreeRTOS cung cấp xSemaphoreGiveFromISR() (Binary Semaphore) dùng trong ISR , nhưng hoàn toàn không có cho **Mutex**
- **FreeRTOS: cấm dùng Mutex trong ISR.**
- Trong ISR ta dùng **Binary Semaphore** dùng để "đánh thức" task từ ISR.
```c
void EXTI0_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xBinarySem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```
- Dùng Queue (FromISR API) để gửi dữ liệu từ ISR sang task
```c
xQueueSendFromISR(queue, &value, &xHigherPriorityTaskWoken);
```
- **Luật 100% đúng trong FreeRTOS**
- "Mutex = Task-only"
-  "Binary Semaphore / Queue = ISR"


## Ảnh chụp 

