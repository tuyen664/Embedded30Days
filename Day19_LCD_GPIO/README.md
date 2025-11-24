# LCD 4-bit (GPIO)

## Mô tả ngắn

- Code điều khiển một màn hình LCD chuẩn HD44780 ( 20×4) bằng chế độ 4-bit song song (D4–D7)
- Các chân dùng GPIO (không phải I2C)
- Mỗi byte (lệnh hoặc dữ liệu) được gửi thành hai nửa (high nibble rồi low nibble); mỗi nửa được "latch" bằng một xung EN.


## Sơ đồ chân (theo code)

```c
#define LCD_RS   (1U << 2)   // PA2
#define LCD_EN   (1U << 3)   // PA3
#define LCD_D4   (1U << 4)   // PA4
#define LCD_D5   (1U << 5)   // PA5
#define LCD_D6   (1U << 6)   // PA6
#define LCD_D7   (1U << 7)   // PA7
#define LCD_PORT GPIOA
```
- LCD VCC phải được cấp 5V ngoài , GND chung với MCU

## Giải thích 

**1. LCD_GPIO_Init()**
- Bật clock cho GPIOA
- Cấu hình PA2..PA7 là Output push-pull, MODE = 50MHz (MODE=11, CNF=00)
- Clear các pin đầu ra (BRR) để đảm bảo RS, EN, D4..D7 = 0 lúc khởi động
- **Lưu ý:** nếu để nhầm Open-Drain hoặc input — LCD sẽ không phản hồi

**2. LCD_Enable()**
```c
BSRR = LCD_EN;   // EN = 1
DelayMs(1);
BRR  = LCD_EN;   // EN = 0
DelayMs(1);
```
- EN = 1 → 0 là **phép latch:** LCD lấy dữ liệu trên D4–D7 khi EN chuyển từ 1 về 0
- Lưu ý: xung EN phải đủ dài; nếu quá ngắn màn hình không nhận lệnh. Delay ~1 ms 

**3. LCD_Send4Bit(uint8_t data)**
- Clear D4–D7 
- Kiểm tra từng bit high (bit 4..7) của data, set tương ứng D4..D7.
- Gọi LCD_Enable() để latch nửa byte
- LCD_Send4Bit() nhận một byte nhưng chỉ dùng các bit 4..7 : dùng cho cả high nibble và low nibble

**4. LCD_SendCmd(uint8_t cmd) và LCD_SendData(uint8_t data)**
- **LCD_SendCmd:**
- RS = 0 (BRR RS)
- Gửi cmd & 0xF0 bằng LCD_Send4Bit() (high nibble)
- Gửi (cmd << 4) & 0xF0 bằng LCD_Send4Bit() (low nibble)
- Delay 2 ms
- **LCD_SendData:**
- RS = 1 (BSRR RS)
- Gửi high nibble rồi low nibble như trên
- Delay 2 ms
- **lưu ý** : RS=1 khi gửi dữ kí tự  hoặc RS=0 khi gửi lệnh. Nếu nhấm sẽ khiến LCD hiển thị vô lý hoặc bỏ lệnh.

**5. LCD_SetCursor(int8_t row, int8_t col)**
- Giới hạn row trong 0..3 và col trong 0..19
- Quy ước địa chỉ DDRAM (HD44780 20×4):
- row0 start = 0x80
- row1 start = 0xC0
- row2 start = 0x94
- row3 start = 0xD4
- Tính pos = start + col, rồi LCD_SendCmd(pos)
- **Lưu ý** : địa chỉ DDRAM không liên tục theo dòng trong mô tả vật lý

**6. LCD_Print()**
- Gọi LCD_SendData() lần lượt cho từng ký tự trong chuỗi
- Con trỏ tự dịch theo lệnh Entry Mode (đã set trong init)

**7. LCD_Init()**
1. LCD_GPIO_Init()
2. Delay 50 ms (đợi LCD khởi động)
3. Gửi 0x30 3 lần bằng LCD_Send4Bit(0x30)
4. Gửi 0x20 để chuyển sang 4-bit mode
5. Gửi 0x28 → Function set: 4-bit, 2 lines (logic), 5×8 font
6. Gửi 0x0C → Display ON, Cursor OFF ,blink cursor off
7. Gửi 0x06 → Entry mode (cursor tự động dịch )
8. Gửi 0x01 → Clear display (delay lâu hơn: ~5 ms)

**8. main**
```c
LCD_Init();

LCD_SetCursor(0, 0);
LCD_Print("STM32 LCD 20x4");

LCD_SetCursor(1, 0);
LCD_Print("GPIO Mode Demo");

LCD_SetCursor(2, 0);
LCD_Print("No I2C Needed!");

LCD_SetCursor(3, 0);
LCD_Print("4-bit Parallel OK!");
```
- đoạn in 4 dòng


**9. Các lưu ý quan trọng**

- LCD 4-bit KHÔNG BAO GIỜ dùng bit thấp (D0–D3) → Vì vậy mọi thứ phải dồn lên 4 bit cao (D7–D4).
- LCD chỉ lấy dữ liệu khi EN chuyển từ 1 → 0 → Nếu EN quá ngắn hoặc không xuống mức thấp đúng lúc → LCD không nhận lệnh.
- Lệnh Clear (0x01) và Home (0x02) cần delay dài (≥ 1.52ms)
- Khi gửi low-nibble, phải shift trái 4 bit
-> Vì D7–D4 của LCD là 4 bit cao của byte bạn viết vào. 
- LCD INIT là phần khó nhất : Nếu gửi sai 1 bước → LCD không vào chế độ 4-bit
- Địa chỉ DDRAM của HD44780 KHÔNG liên tục theo dòng
-> 20×4 map theo: 0x80, 0xC0, 0x94, 0xD4
- LCD phải dùng nguồn 5V ngoài
- Không được chạy EN quá nhanh 
- BSRR và BRR là các thanh ghi bắt buộc phải dùng
-> Không được dùng ODR kiểu LCD_PORT->ODR |= ..., dễ gây lỗi. 
- ví dụ LCD_PORT->ODR |= (1 << D4);  // set bit cần 1 ,  Nhưng còn các bit cần 0 thì sao?
- OR không thể “ép” chúng về 0 được,Chúng sẽ mang giá trị rác (giá trị cũ từ chu kỳ trước). 

- Các bước CPU thực sự khi phép toán ODR |= (1<<x) là:
- CPU đọc ODR (toàn 16 bit)
- CPU sửa 1 bit
- CPU ghi lại ODR (toàn 16 bit)
- Các chân không liên quan cũng bị ghi lại
- Nếu thời điểm đó một ngắt đang thay đổi ODR → glitch
- Nếu pin khác đang dùng cho UART/SPI/I2C/ADC — rất nguy hiểm 


## Ảnh chụp 

