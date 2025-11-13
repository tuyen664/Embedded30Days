## Mục Tiêu

 - Biết cấu hình ADC cho 3 kênh cho 3 chân PA0-PA1-PA2
 - Tăng độ chính xác của dữ liệu đọc bằng cách lấy trung bình 5 mẫu dữ liệu 
 - Gửi dữ liệu đọc được lên màn hình Tera Term qua UART

## Giải thích 

### 1. Cấu hình ADC
 - Enable Clock cho GPIOA (bit2) và ADC1 (bit 9) trong thanh ghi RCC->APB2ENR |= (1U << 2) | (1U << 9);
 - Cấu hình các chân PA0, PA1, PA2 làm ngõ vào **analog** , khi clear hết 4 bit → MODE=00, CNF=00 → chế độ **analog input**
 - Bật ADC : ADC1->CR2 |= (1U << 0);  // ADON = 1 -> bật nguồn
 - khi bật lần đầu, ADC chưa sẵn sàng ngay mà cần một thời gian để ổn định : DelayMs(1);
 - Lần ghi ADON đầu tiên là bật nguồn ADC, lần ghi thứ hai chỉ cần thiết nếu muốn bắt đầu chuyển đổi ngay (khi chưa dùng SWSTART).
 - Ở đây dùng SWSTART, nên chỉ cần ghi 1 lần là đủ.

 - Bật chế độ chuyển đổi liên tục: ADC1->CR2 |= (1U << 1);   // CONT = 1 (Continuous mode)
 - Nếu CONT=1, ADC tự động khởi động chuyển đổi mới sau mỗi lần kết thúc (EOC)
 - Ta vẫn cần lệnh SWSTART lần đầu để bắt đầu chuỗi liên tục.

 - ADC tự động chạy liên tục (sau khi kích hoạt lần đầu bằng SWSTART)
 - Nếu CONT = 0 thì ADC chỉ chuyển đổi 1 lần duy nhất rồi dừng
 - Chọn trigger khởi động là phần mềm : EXTSEL[19:17]: chọn nguồn kích hoạt chuyển đổi ADC.
 - Ở đây chọn 111 (SWSTART) → kích hoạt bằng phần mềm : ADC1->CR2 |= (7U << 17);
 - EXTTRIG (bit 20) bị tắt vì ta không dùng trigger ngoài (nút, timer...).
 -> Kết quả: chỉ cần gọi ADC1->CR2 |= (1<<22) để bắt đầu chuyển đổi ADC

 - Reset bộ hiệu chuẩn ADC : ADC1->CR2 |= (1U << 3); Bit 3 (RSTCAL) = 1 → reset calibration registers.
 - Khi phần cứng đang reset, bit này = 1. Vòng while chờ đến khi bit tự clear (xong reset).while (ADC1->CR2 & (1U << 3));
 -> Đảm bảo các thanh ghi hiệu chuẩn ADC sạch trước khi calibrate lại.

 - Hiệu chuẩn ADC (calibration) : ADC1->CR2 |= (1U << 2);  // CAL = 1 , Bit 2 (CAL) = 1 → bắt đầu quá trình hiệu chuẩn nội bộ (calibration)
 - ADC sẽ tự đo và hiệu chỉnh sai lệch phần cứng bên trong. Chờ cho đến khi bit CAL tự về 0 (cal xong).
 -> Nếu bỏ bước này, ADC sẽ đọc sai lệch nhiều.
 
 **- Tóm tắt lại các bước khởi tạo ADC** : 
 - Bật clock GPIOA, ADC1
 - Set PA0–PA2 analog
 - Bật ADC (ADON). Delay 1ms 
 - Bật chế độ Continuous -> ADC chạy liên tục
 - Chọn SWSTART trigger -> cho phép start bằng phần mềm
 - Reset calibration
 - Start calibration
 - Kết thúc khi CAL=0 -> ADC sẵn sàng hoạt động

#### 1.1 Đọc dữ liệu ADC : static uint16_t ADC1_Read(uint8_t channel)
 - Thanh ghi SQR3 (Regular Sequence Register 3) dùng để chọn kênh ADC
 - ADC1->CR2 |= (1U << 22); // Bit 22 trong CR2 là SWSTART. Khi bit này được set = 1 → bắt đầu quá trình chuyển đổi ADC
 - while (!(ADC1->SR & (1U << 1))); Vòng lặp này chờ đến khi việc chuyển đổi xong.
 - Bit 1 trong ADC1->SR là EOC (End of Conversion) .Khi ADC hoàn tất đo, EOC=1 .
 - return (uint16_t)ADC1->DR; // Đọc dữ liệu kết quả từ thanh ghi Data Register (DR) , Khi đọc xong, bit EOC tự động bị xóa bởi phần cứng.
 - Kết quả: Trả về giá trị ADC 12-bit (0–4095). DR chỉ 16 bit hợp lệ (ADC 12-bit, phần cao = 0).

#### 1.2 Đọc trung bình 5 mẫu dữ liệu để chính xác hơn : static uint16_t ADC1_ReadAverage(uint8_t channel, uint8_t sample_count)
 - Tham số : **channel** → chỉ định kênh ADC muốn đọc (VD: 0 = PA0, 1 = PA1, v.v.)
 - **sample_count** → số lượng mẫu ta muốn lấy để tính trung bình
 - Biến sum dùng để cộng dồn tất cả các mẫu ADC. Dùng kiểu uint32_t (32-bit) để tránh tràn số khi cộng nhiều mẫu.
 - sum += ADC1_Read(channel); Cộng dồn các giá trị đọc được từ ADC vào sum
 - return (uint16_t)(sum / sample_count); // Chia tổng sum cho sample_count → lấy giá trị trung bình.Ép kiểu về uint16_t vì kết quả luôn nằm trong 0–4095

-> Kết quả trả về là giá trị ADC trung bình của kênh đó.  

### 2. Khởi tạo UART
 - void USART1_Init(void);
 + Bật clock: GPIOA + USART1
 + PA9 TX → Alternate Function Push-Pull, 50 MHz
 + PA10 RX → Input Floating mode
 + Baudrate: 9600
 + Bật UART (UE), truyền (TE), nhận (RE)

### 2.1 Gửi kí tự và chuỗi
 - void USART1_SendChar(char c); // gửi 1 ký tự
 - void USART1_SendString(const char *s); // gửi chuỗi
 
## 3. Systick
 - void SysTick_Init(void);  // Cấu hình **1ms tick**
 + Với HCLK = 72 MHz:
 + SysTick->LOAD = 72000 - 1; // tạo ngắt mỗi 1 ms
 + SysTick->CTRL = 7; // bật SysTick, nguồn từ HCLK, cho phép ngắt

 - void SysTick_Handler(void); // tăng biến đếm ms
 - Tạo hàm Delay_ms() :  Dùng biến start lưu biến sysTickMs ban đầu , sau mỗi ms biến này được tăng lên 1
 - Đợi đến khi khoảng thời gian cần delay đạt đến thì tự thoát hàm while ((sysTickMs - start) < ms)

## Hình Ảnh 
