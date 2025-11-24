## Tổng Quan

- UART TX/RX bằng interrupt (không dùng HAL)
- Queue để truyền dữ liệu giữa ISR ↔ task
- Timer để gửi message
- Task để nhận lệnh UART (command)
- Task điều khiển LED theo command
- ISR (RX) → Queue → Task_UART_Receiver → Queue → Task_LED_Control → LED                                     

- ISR RX: nhận từng ký tự → chuyển vào queue RX
- Task_UART_Receiver: ghép ký tự thành command → gửi command sang queue Cmd
- Task_LED_Control: nhận command → điều khiển LED + trả lời
- Timer: 1 giây in "[SYS] alive..."
- interrupt UART TX : Task gọi UART1_SendChar_IT , ISR TX lấy data từ queue TX → gửi ra Tera Term

## Các lưu ý trong lúc cấu hình UART
```C
USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;
USART1->CR1 &= ~USART_CR1_TXEIE; // Disable TX interrupt initially

NVIC_SetPriority(USART1_IRQn, 8);
NVIC_EnableIRQ(USART1_IRQn);
```
- USART_CR1_RXNEIE  : Bật interrupt nhận dữ liệu
- USART1->CR1 &= ~USART_CR1_TXEIE; -> Tắt TXE interrupt ban đầu
- Vì TXE interrupt chạy khi thanh ghi gửi trống. Nếu bật ngay từ đầu mà không có gì để gửi → nó spam interrupt liên tục.

- Ta chỉ bật TXEIE khi có dữ liệu cần truyền (trong hàm queue ISR TX)

- NVIC_SetPriority(USART1_IRQn, 8); -> mức ưu tiên interrupt phải lớn hơn **configMAX_SYSCALL_INTERRUPT_PRIORITY**
**1. RX interrupt RXNEIE chạy mỗi khi nhận 1 byte**
- ISR này chạy khi Một byte vừa nhận xong , USART_SR_RXNE = 1 
- Nếu tốc độ baud cao hoặc task xử lý chậm → overrun (ORE) → mất dữ liệu
**2.TXE interrupt nguy hiểm nếu bật sai thời điểm**
- Nếu bật TXEIE khi TX buffer rỗng → CPU bị spam interrupt → treo hệ thống
-> bật TXEIE chỉ khi có dữ liệu để gửi  

## UART1_SendChar_IT()
```C
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
```
- Hàm này gửi **1 ký tự** qua UART ở chế độ interrupt
- Cách hoạt động : 
- Nếu UART đang rảnh → ghi thẳng vào thanh ghi DR để gửi ngay
- Nếu UART đang bận → đưa ký tự vào queue (hàng đợi) để ISR gửi dần
- ISR TX sẽ tự động gửi các ký tự còn lại trong queue mỗi lần TXE interrupt xảy ra.
**1.Check queue**
```c
if (xQueueTxChar == NULL) return;
```
→ Nếu queue chưa tạo → thoát để tránh crash.
**2.taskENTER_CRITICAL()**

- Bảo vệ vùng code khỏi : ISR , task khác , race condition khi truy cập queue hoặc thanh ghi UART
- Đây là điểm **rất quan trọng** trong RTOS khi làm UART DMA/Interrupt
**3. Nếu UART rảnh → gửi ngay lập tức**
```c
if ((USART1->SR & USART_SR_TXE) && !(USART1->CR1 & USART_CR1_TXEIE))
{
    USART1->DR = c;
    USART1->CR1 |= USART_CR1_TXEIE;
}
```
- TXE = 1 → Thanh ghi truyền trống, UART đang rảnh
- TXEIE = 0 → Interrupt chưa bật → chứng tỏ UART idle
-> Viết trực tiếp vào thanh ghi DR để gửi ngay kí tự đầu tiên 
- Bật TXE interrupt để ISR gửi ký tự tiếp theo từ queue
**4. Nếu UART đang bận → đưa vào queue**
```c
xQueueSend(xQueueTxChar, &c, 0);
USART1->CR1 |= USART_CR1_TXEIE;
```
- UART đang gửi → không thể ghi ngay vào DR
- Store vào queue → ISR sẽ lo gửi tiếp
- 0 = không chờ một tick nào → nếu queue đầy → return ngay lập tức

**5. taskEXIT_CRITICAL()**
- Kết thúc critical section
**6. Các lưu ý quan trọng**
- Ta phải check TXE và TXEIE cùng lúc , vì TXE có thể = 1 nhưng UART vẫn có thể đang bận (ISR đang xử lý)
- Nếu ta ghi DR lúc đó → gây gửi trùng hoặc mất byte

- Ta phải bật TXEIE khi push queue , nếu không bật : ISR sẽ không chạy -> Byte trong queue sẽ không bao giờ gửi -> UART treo

- ISR mỗi lần TXE xảy ra sẽ : Lấy 1 ký tự từ queue ,  Ghi vào DR , Khi queue hết → tắt TXEIE để dừng interrupt

- Interrupt TX chỉ xảy ra khi TXE chuyển từ 0 → 1 trong lúc TXEIE = 1 (tức là phỉa gửi tay thủ công kí tự đầu tiên để tạo cạnh 0-> 1) (từ "có dữ liệu" → "trống")

## USART1_IRQHandler()

- Mục đích của ISR này : 
- Nhận dữ liệu UART (RX) và gửi vào queue cho task xử lý
- Lấy dữ liệu từ queue và gửi UART từng byte (TX)
- Nếu có task ưu tiên cao hơn đang chờ, ISR sẽ kích hoạt **context switch** ngay sau khi thoát interrupt.
**1. Khởi tạo biến để kiểm tra có cần switch context không**
```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
```
- ISR không được tự tiện gọi scheduler, mà phải ghi nhận “có task ưu tiên cao cần chạy ngay hay không”
**2. Kiểm tra ngắt RX**
```c
if (USART1->SR & USART_SR_RXNE)
{
    char c = (USART1->DR & 0xFF); // đọc DR sẽ tự động clear RXNE
    if (c >= 'a' && c <= 'z') c -= 32; // chuyển sang uppercase
    xQueueSendFromISR(xQueueRxChar, &c, &xHigherPriorityTaskWoken);
}
```
- RXNE =1: nghĩa là 1 byte mới đã được nhận và nằm trong DR -> kích hoạt ngắt
- Đọc DR sẽ tự động CLEAR cờ RXNE.
- Chuyển kí tự sang chữ in hoa
- Gửi vào queue cho task xử lý
- Không bao giờ delay trong ISR
- Không được dùng xQueueSend() mà phải dùng xQueueSendFromISR()
**3. TX interrupt**
```c
if ((USART1->SR & USART_SR_TXE) && (USART1->CR1 & USART_CR1_TXEIE))
{
    char txChar;
    if (xQueueReceiveFromISR(xQueueTxChar, &txChar, &xHigherPriorityTaskWoken) == pdPASS)
    {
        USART1->DR = txChar;
    }
    else
    {
        USART1->CR1 &= ~USART_CR1_TXEIE; // tắt TXE interrupt khi queue hết
    }
}
```
- check : TXE =1 , TXEIE =1 (UART đang rảnh và cho phép TX interrupt chạy)
- Nhận 1 byte từ queue TX: 
- Thành công → ghi vào DR để gửi 
- Queue trống → TẮT TXEIE để không bị spam ngắt (TXE interrupt chạy liên tục khi TX queue trống)
- Quy trình truyền data : push byte đầu → enable TXEIE → ISR tự lo gửi tiếp
- khi ta ghi USART1->DR = c; -> c được copy từ DR → Shift Register ngay lập tức (hardware làm)
- DR trống lại ngay sau khi shift register nhận xong → sẵn sàng nhận byte tiếp theo
- DR chỉ là “bộ đệm 1 byte” , Shift register là nơi UART gửi bit ra dây TX

```c
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```
- Nếu ISR làm một task priority cao hơn “tỉnh dậy”, FreeRTOS sẽ switch ngay. 
**4. Tóm tắt**
- RX: UART nhận byte → kích ISR → ISR gửi vào queue → task xử lý
- TX: Task gửi chuỗi → task gửi byte đầu tiên vào queue → enable TXE interrupt →
  ISR tự lấy TX queue gửi từng byte → hết → ISR tắt TXE interrupt.

## Task_UART_Receiver

- Nhận từng ký tự từ queue xQueueRxChar
- Echo lại ký tự (gửi lại ra UART cho người dùng thấy)
- Gom ký tự thành command line ( "LED ON")
- Khi gặp \r hoặc \n → kết thúc chuỗi, gửi nguyên lệnh sang **queue xQueueCmd** cho task khác xử lý
- Điều khiển timer (xTimerMonitor) để tránh khi đang gõ lệnh thì timer xen ngang.

**1. BUFFER – nơi lưu command**
```c
char buffer[CMD_BUF_LEN];
uint8_t idx = 0;
char c;
```
- buffer: Lưu các ký tự người dùng gõ
- idx: Vị trí đang ghi
- c: Ký tự nhận từ queue
- Vì sao buffer nằm bên trong task là an toàn?
- Buffer nằm trong stack của task , Và chỉ task này được dùng buffer 

**2. Vòng lặp task**
```c
for (;;)
{
    if (xQueueReceive(xQueueRxChar, &c, portMAX_DELAY) == pdPASS)
    {
```
- Task block vô hạn (portMAX_DELAY) cho đến khi có ký tự mới
**3. Echo ký tự lại UART**
```c
UART1_SendChar_IT(c);
```
- Gửi ký tự lại ngay lập tức để người dùng thấy mình đã gõ gì
**4. Xử lý chuỗi**
```c
if (idx == 0 && c != '\r' && c != '\n')
{
    xTimerStop(xTimerMonitor, 0);
}
```
- Ngay cả khi ta chưa gõ gì, ISR không push vào queue → task block → không thực thi.
- Khi ta gõ một ký tự → c = ký tự gõ, ví dụ 'A' thì nó mới xem xét đến điều kiện này.
- Khi người dùng bắt đầu gõ ký tự đầu tiên cho command mới : Ta stop timer monitor , Timer này sẽ được start lại khi command hoàn thành
- Khi đang gõ chữ , có thể bị xen ngang bởi Timer
**5. Khi gặp ENTER (\r , \n)**
```c
if (c == '\r' || c == '\n')
{
    buffer[idx] = '\0';
    if (idx > 0)
    {
        xQueueSend(xQueueCmd, buffer, 0);
        idx = 0;
        xTimerStart(xTimerMonitor, 0);
    }
}
```
- Kết thúc chuỗi bằng \0
- Nếu command KHÔNG rỗng (idx > 0) → Gửi chuỗi sang queue xQueueCmd cho task khác xử lý
- Reset index về 0 để chuẩn bị command mới
- Start lại timer
- xQueueSend(..., 0) = không block -> trong design này task xử lý command luôn phải chạy nhanh

**6. Ghi ký tự vào buffer**
```c
else if (idx < CMD_BUF_LEN - 1)
{
    buffer[idx++] = c;
}
```
- Luôn để lại 1 byte cuối cho ký tự kết thúc chuỗi (\0)

## Task_LED_Control()
- Đợi (block) để nhận lệnh từ queue xQueueCmd
- Mỗi lệnh là 1 chuỗi: "ON", "OFF", "TOGGLE"
- Sau đó thao tác LED + gửi phản hồi UART
- xQueueReceive(xQueueCmd, cmd, portMAX_DELAY)
- Task block vô hạn cho đến khi có lệnh trong queue
- Khi có dữ liệu, FreeRTOS copy toàn bộ data vào buffer cmd
- Nếu queue không bao giờ gửi lệnh → task này ngủ mãi, không tốn CPU
- Queue KHÔNG truyền pointer của chuỗi, mà copy toàn bộ chuỗi vào buffer

## TimerMonitor để làm gì trong thực tế ?
- Cảnh báo user nhập lâu quá
- Reset buffer nếu user bỏ dở
- Tự động gửi command nếu timeout



## Ảnh chụp 

