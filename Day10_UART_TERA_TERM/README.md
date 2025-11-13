# STM32 UART Command LED Controller

## Mục tiêu 

 - Gửi và nhận lệnh qua UART (Tera Term)
 - Điều khiển LED (PC13) theo lệnh : 
 + ON: Bật LED
 + OFF : Tắt LED
 + BLINK: Nháy LED
 + STATUS: Xem trạng thái LED
 - Mọi ký tự chữ thường (a–z) gửi từ Tera Term xuống MCU sẽ tự động được chuyển thành chữ hoa (A–Z) khi MCU xử lý lệnh
 - Timeout / non-blocking Delay qua systick
 
## 1. Cấu trúc dự án
 - **main.c** -> Vòng lặp chính , đọc lệnh UART , điều khiển LED
 - **led.c/led.h** -> khởi tạo và thực thi điều khiển LED
 - **uart.c/uart.h** -> khởi tạo uart , gửi ký tự từ Tera Term sang MCU tạo lệnh điều khiển
  và gửi kí tự từ MCU hiển thị lên Tera Term.
 - **systick.c/systick.h** -> Bộ đếm 1ms , cung cấp hàm delay không chặn và timeout

## 2. Module LED
 - **#ifndef LED_H** : Nếu LED_H chưa được định nghĩa -> tránh việc header file bị include nhiều lần trong cùng một dự án.
 + Khi trình biên dịch gặp #include "led.h" lần đầu, LED_H chưa được định nghĩa → phần code sau nó sẽ được đọc và biên dịch.
 + Nếu có file khác include lại led.h, compiler sẽ thấy LED_H đã được định nghĩa → bỏ qua nội dung bên trong.
 -> #ifndef là cái cổng bảo vệ để header file chỉ được đọc một lần, tránh lỗi trùng định nghĩa.

 - **#define LED_H** : : định nghĩa macro LED_H
 - **#endif** : kết thúc khối điều kiện #ifndef
 - Tất cả nội dung của led.h nằm giữa **#ifndef LED_H** và **#endif** sẽ chỉ được biên dịch một lần.
 
 - static volatile uint8_t ledState; // 0 = OFF, 1 = ON
 - void LED_Init(void);           // Khởi tạo GPIO PC13
 - void LED_ON(void);             // Bật LED
 - void LED_OFF(void);            // Tắt LED
 - void LED_TOGGLE(void);         // Đảo trạng thái LED
 + if (ledState) LED_OFF(); else LED_ON(); // nếu LED đang on (ledstate = 1) thì tắt và ngược lại

 - uint8_t IS_LED_ON(void);       // Kiểm tra trạng thái LED

 -> static inline là hàm được biên dịch trực tiếp tại chỗ gọi (inline) và chỉ nhìn thấy trong file hiện tại (static)
   giúp tối ưu tốc độ và tránh xung đột tên hàm giữa các file.
 -> static inline thường đặt trong file header cho các hàm nhỏ (ví dụ IS_LED_ON())

## 3. Module UART

### 3.1 Khởi tạo UART
 - void USART1_Init(void);
 + Bật clock: GPIOA + USART1
 + PA9 TX → Alternate Function Push-Pull, 50 MHz
 + PA10 RX → Input Floating mode
 + Baudrate: 9600
 + Bật UART (UE), truyền (TE), nhận (RE)

### 3.2 Gửi kí tự và chuỗi
 - void USART1_SendChar(char c); // gửi 1 ký tự
 - void USART1_SendString(const char *s); // gửi chuỗi

### 3.3 Nhận ký tự
 - Nhận 1 ký tự từ UART1 với timeout tính bằng ms.
 - Nếu không có dữ liệu trong khoảng thời gian timeout_ms, trả về -1
 - Tất cả ký tự chữ thường (a-z) được tự động chuyển thành chữ hoa (A-Z) trước khi trả về.
 
 - Lấy thời gian bắt đầu: start = GetTick()
 - Chờ cho đến khi có dữ liệu (RXNE = 1) hoặc hết timeout: 
   + while (!(USART1->SR & (1 << 5))) {  if ((GetTick() - start) >= timeout_ms) return -1;}
 - Khi có dữ liệu:  Đọc ký tự từ thanh ghi dữ liệu: c = USART1->DR & 0xFF , Nếu ký tự là 'a'..'z', chuyển sang 'A'..'Z'.
 - Trả về ký tự đã xử lý (kiểu int)
 -> Lưu ý Khi nhận dữ liệu, cần kiểm tra if (rc < 0) trước khi ép kiểu sang char, tránh mất giá trị khi bị timeout (-1).
 
## 4. Module systick
 - void SysTick_Init(void);  // Cấu hình **1ms tick**
 + Với HCLK = 72 MHz:
 + SysTick->LOAD = 72000 - 1; // tạo ngắt mỗi 1 ms
 + SysTick->CTRL = 7; // bật SysTick, nguồn từ HCLK, cho phép ngắt

 - void SysTick_Handler(void); // tăng biến đếm ms
 - uint32_t GetTick(void);       // Lấy thời gian hiện tại (ms)
 - uint8_t Delay_Elapsed(uint32_t start, uint32_t ms) // kiểm tra xem có vượt timeout không
 - Biến toàn cục: volatile uint32_t sysTickMs; // Đếm ms từ khi khởi tạo
 -> Sử dụng để:  Delay không chặn LED , Timeout nhận UART , Blink LED định kỳ.

## 5. Vòng lặp chính (main.c)

### 5.1 Khởi tạo
 
 SysTick_Init();
 LED_Init();
 USART1_Init();
 USART1_SendString("UART Command Mode Ready\r\n");
 
### 5.2 Vòng lặp 
 **1. Blink LED không chặn**
 if (blinkMode && Delay_Elapsed(blinkLast, 500))
        {
            LED_TOGGLE();
            blinkLast = GetTick();
        }
 Cứ mỗi 500ms , led nháy 1 lần , nếu ta bật chế độ Blink (blinkMode = 1)
 - blinkMode được bật khi nhận lệnh "BLINK", tắt khi nhận "OFF" hoặc "ON"
 - khi blinkMode = 1, LED sẽ tự động nháy mỗi 500ms mà không cần delay blocking

 **2. Nhận lệnh UART**
 - int rc = USART1_ReceiveChar(10); // timeout 10ms
 - if(rc < 0) continue;             // chưa nhận gì 
 - Chuyển kết quả nhận được sang kí tự : char c = (char)rc;
 - Ghi từng kí tự c vào mảng buffer chi đến khi gặp kí tự '\r' hoặc '\n'
 + else if (idx < sizeof(buffer) - 1) buffer[idx++] = c;
 - Loại bỏ ký tự CR, LF, whitespace cuối
 if (c == '\r' || c == '\n');

 - So sánh với các lệnh : 'ON' , 'OFF' , 'BLINK' , 'STATUS'
 - Gửi phản hồi về Tera Term khi lệnh đúng hoặc sai lệnh
 - Xóa mảng buffer : memset(buffer, 0, sizeof(buffer));

## 6. Cách sử dụng 

 1. Build code trên STM32F103 (Blue Pill)
 2. Kết nối Tera Term: Baudrate: 9600 , và kết nối đúng cổng COM đã cắm UART
 3. Nhấn reset trên STM32F103 , MCU sẽ gửi : "UART Command Mode Ready" lên Tera Term
 4. Chuyển về chế độ gõ Tiếng Anh trước khi gõ , Gõ các lệnh :
 + ON: Bật LED
 + OFF : Tắt LED
 + BLINK: Nháy LED
 + STATUS: Xem trạng thái LED
 5. LED phản hồi theo lệnh
 6. Tất cả lệnh không phân biệt chữ hoa/thường

## Ảnh chụp

