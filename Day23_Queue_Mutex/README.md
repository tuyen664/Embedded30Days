## Tổng Quan
- Giao tiếp giữa các task bằng Queue và Mutex (Semaphore)
- Task_Button: Đọc trạng thái nút PA1, debounce 50 ms, gửi sự kiện nhấn nút vào queue cho task khác xử lý.
- Task_Button_UART: Nhận sự kiện từ queue, in thông báo “Button Pressed!” qua UART và toggle LED, sử dụng mutex bảo vệ UART.
- Task_Semaphore: Mỗi 2 giây in “System alive!” qua UART, dùng mutex để tránh xung đột với task UART khác.
- Task_LED: Nháy LED PC13 mỗi 500 ms

## Task_Button
```c
static void Task_Button(void *pvParameters)
{
    (void)pvParameters;
    uint8_t msg = 1;
    uint8_t prev = 1;
    const TickType_t debounce = pdMS_TO_TICKS(50);

    for (;;)
    {
        uint8_t curr = (GPIOA->IDR & (1 << BUTTON_PIN)) ? 1 : 0;
        if (prev == 1 && curr == 0)
        {
            xQueueSend(xQueueButton, &msg, 0); // 0 - khong doi
        }
        prev = curr;
        vTaskDelay(debounce);
    }
}
```
**1. Tổng quan**
- Hàm nay đọc nút nhấn PA1 , phát hiện sườn xuống (1 -> 0) , debounce 50ms , gửi 1 message (1 byte) vào queue khi nút được nhấn
- không chờ queue -> xử lý nhanh , tránh blocking
- **uint8_t msg = 1;** -> giá trị được gửi vào queue khi nút được nhấn , đây chỉ là **event flag** , không mang nhiều thông tin
- Trong thực tế có thể gửi : ID của sensor , state của button ,...

- **uint8_t prev = 1;** 
- Vì input PA1 được cấu hình pull-up, trạng thái bình thường = 1
- Khi bấm nút → kéo xuống GND → thành 0 
-> prev giúp ta phát hiện sườn thay vì đọc liên tục → tránh trigger nhiều lần.
```c
const TickType_t debounce = pdMS_TO_TICKS(50);
```
- debounce delay = 50ms
- Nếu **configTICK_RATE_HZ = 1000** , 1 tick = 1ms
  
**2. Vòng for chính**
```c
uint8_t curr = (GPIOA->IDR & (1 << BUTTON_PIN)) ? 1 : 0;
```
- Đọc trạng thái chân PA1
- GPIOA->IDR: Input Data Register :  1 -> thả , 0 -> bấm
- Phát hiện sườn xuống : prev = 1 , curr = 0 ;
```c xQueueSend(xQueueButton, &msg, 0); ```
- 0 tick wait → Non-blocking send : Nếu queue còn chỗ → OK , Nếu queue đầy → trả về fail ngay , Task không bao giờ block.
-> vì task này có priority cao hơn UART task nên đúng
-> Lỗi tiềm ẩn : TA KHÔNG check return value : Nếu queue full → event bị mất mà ta không biết
- vTaskDelay(debounce); Vì poll mỗi 50ms → mọi nhiễu cơ khí (5–20ms) sẽ được lọc tự nhiên
- Nếu nhấn rất nhanh < 50ms → có thể không nhận.
- Nếu nhấn giữ → chỉ nhận 1 event, vì chỉ có 1 lần từ prev=1 → curr=0.

**3.Cải tiến thêm**

**3.1 Kiểm tra queue full**
```c
if (xQueueSend(xQueueButton, &msg, 0) != pdPASS)
{
    // Queue full → xử lý
}
```
**3.2 Gửi nhiều loại event**
```c
typedef struct {
    uint8_t type; // press, release, double
    uint32_t time;
} ButtonEvent;
```
**3.3 Dùng interrupt thay polling**

**4. Lưu ý quan trọng**
- Task chạy từ đầu hàm CHỈ 1 LẦN → sau đó mỗi lần scheduler quay lại, nó chỉ chạy tiếp cái phần nó dừng.
- Và ở đây là vòng for () , nó không bao giờ quay lại  đoạn đầu tiên 
- Khi ta bấm nút → task chạy nhiều vòng FOR liên tiếp
- Mỗi 50 ms (debounce), task được scheduler đánh thức và chạy lại vòng for(;;)
- Trong mỗi vòng : curr = đọc nút , prev = giá trị curr của vòng trước
- Nếu ta giữ nút : curr = 0 , prev = 0  (từ vòng thứ 2 trở đi) → Không vào if nữa
- Khi ta thả nút → curr trở lại 1 → prev được cập nhật lên 1
- ví dụ code Phát hiện sườn xuống (nhấn) và  Phát hiện sườn lên (nhả)
```c
static void Task_Button(void *pvParameters)
{
    (void)pvParameters;

    uint8_t prev = 1;   // 1 = thả, 0 = nhấn
    const TickType_t debounce = pdMS_TO_TICKS(30);

    for (;;)
    {
        uint8_t curr = (GPIOA->IDR & (1 << BUTTON_PIN)) ? 1 : 0;

        /* ===== Detect Falling Edge (1 → 0) ===== */
        if (prev == 1 && curr == 0)
        {
            uint8_t msg = 0;   // 0 = FALLING (nhấn)
            xQueueSend(xQueueButton, &msg, 0);
        }

        /* ===== Detect Rising Edge (0 → 1) ===== */
        else if (prev == 0 && curr == 1)
        {
            uint8_t msg = 1;   // 1 = RISING (nhả)
            xQueueSend(xQueueButton, &msg, 0);
        }

        prev = curr;
        vTaskDelay(debounce);
    }
}
```
- Ví dụ xử lý trong Task_Button_UART 

```c
static void Task_Button_UART(void *pvParameters)
{
    (void)pvParameters;

    uint8_t recv;

    for (;;)
    {
        if (xQueueReceive(xQueueButton, &recv, portMAX_DELAY) == pdPASS)
        {
            if (xSemaphoreTake(xUART_Semaphore, portMAX_DELAY))
            {
                if (recv == 0)
                    UART1_SendString("[BTN] Pressed (Falling)\r\n");
                else if (recv == 1)
                    UART1_SendString("[BTN] Released (Rising)\r\n");

                xSemaphoreGive(xUART_Semaphore);
            }
        }
    }
}
```
## Task_Button_UART
```c
static void Task_Button_UART(void *pvParameters)
{
	  (void)pvParameters;
    uint8_t recv;
    for (;;)
    {
        if (xQueueReceive(xQueueButton, &recv, portMAX_DELAY) == pdPASS)
        {
       
            if (xSemaphoreTake(xUART_Semaphore, portMAX_DELAY) == pdTRUE)
            {
                UART1_SendString("[UART] Button Pressed!\r\n");
                LED_TOGGLE();
                xSemaphoreGive(xUART_Semaphore);
            }
        }
    }
}
```
- Task này làm 2 nhiệm vụ:
- Chờ dữ liệu nút nhấn từ xQueueButton
- Gửi thông báo UART khi có nút nhấn → nhưng phải dùng Mutex để tránh xung đột UART
- Toggle LED để phản hồi sự kiện nút.

**1. Biến nhận dữ liệu queue**
- uint8_t recv; 
- Data từ queue được copy vào biến này.
  
**2. Nhận queue (block cho đến khi có nút nhấn**
```c
if (xQueueReceive(xQueueButton, &recv, portMAX_DELAY) == pdPASS)
```
- portMAX_DELAY nghĩa là: → Task này sẽ ngủ hoàn toàn cho đến khi queue có dữ liệu.
-> Vì thế task này không cần priority cao, nó chỉ chạy khi thật sự cần → rất tối ưu.
- KHÔNG tốn CPU, KHÔNG polling.
- FreeRTOS scheduler chỉ đánh thức task này khi Task_Button gửi msg.
  
**3. Bảo vệ UART bằng mutex (prevent race condition)***
```c
if (xSemaphoreTake(xUART_Semaphore, portMAX_DELAY) == pdTRUE)
```
- ART là resource chung dùng bởi 2 task : Task_Button_UART và Task_Semaphore
- Nếu 2 task cùng lúc in UART → dữ liệu bị trộn lẫn
→ Mutex đảm bảo chỉ 1 task được quyền dùng UART tại 1 thời điểm

- portMAX_DELAY : Nếu UART đang bận → task này sẽ block và nhường CPU

**3. Gửi UART + Toggle LED**

**4. Trả lại mutex**
```c
xSemaphoreGive(xUART_Semaphore);
```
- Nếu ta quên dòng này → UART sẽ bị “lock cứng”, các task khác không bao giờ dùng  UART được nữa.

**5. Lưu ý quan trọng**
- Task_Button_UART là task **event-driven** , Nó KHÔNG chạy liên tục , Nó Chỉ chạy khi có sự kiện nút.
→ Đây là design chuẩn trong FreeRTOS: Task nhẹ , Không chiếm CPU , Không cần priority cao ,Không dùng timer.
- Queue là “interrupt-safe” còn UART không phải
-> Gửi queue từ Task_Button → an toàn, không gây block lâu 
-> Nhưng UART gửi từng byte → chậm → phải được mutex bảo vệ. 

- Mutex có priority inheritance : 
- Nếu Task_Semaphore đang in UART và Task_Button_UART (priority cao hơn) cần in:
→ Task_Semaphore sẽ được tăng priority tạm thời để in cho nhanh rồi give mutex -> Tránh priority inversion

- Việc toggle LED ở đây OK nhưng sẽ xung đột với Task_LED nếu bạn dùng cùng 1 LED.
- Task_LED toggle mỗi 500ms , Task_Button_UART toggle khi nhấn nút

**6. Những lỗi tiềm ẩn**
- Nếu Task_Button gửi quá nhanh → queue chứa nhiều “1” liên tục → event lặp nhiều lần.
- Không xử lý nội dung recv : Trong các project lớn → event type quan trọng

## Task_Semaphore
```c
static void Task_Semaphore(void *pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        if (xSemaphoreTake(xUART_Semaphore, portMAX_DELAY) == pdTRUE)
        {
            UART1_SendString("[SYS] System alive!\r\n");
            xSemaphoreGive(xUART_Semaphore);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```
- Semaphore phải được Take → Give trong thời gian ngắn : Take → làm việc rất nhanh → Give → rồi mới delay
- Semaphore dùng cho tài nguyên không chia sẻ được


## Ảnh chụp 

