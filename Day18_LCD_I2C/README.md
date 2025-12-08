# STM32F103 I2C LCD Demo
## Tổng Quan

Dự án sử dụng **STM32F103** để giao tiếp với **LCD qua I2C (PCF8574)** và debug bằng **UART**

- Khởi tạo : clock, SysTick, UART, I2C
- Tự động phát hiện địa chỉ LCD trên bus I2C
- Hiển thị thông báo trên LCD
- Sử dụng hàm delay dựa trên SysTick
- Thực hành giao tiếp I2C cơ bản với timeout và xử lý lỗi

## 1. Cấu hình I2C1

**1.1 Bật clock GPIOB và I2C1**

- APB2ENR bật clock cho GPIOB ( PB6=SCL, PB7=SDA) , APB1ENR bật clock cho I2C1
- PB6 = SCL: dây đồng hồ do STM32 điều khiển, đồng bộ dữ liệu I2C
- PB7 = SDA: dây dữ liệu 2 chiều, truyền/nhận byte giữa STM32 và LCD

    Cả 2 dùng Open-Drain + pull-up, SCL clock tốc độ truyền 100kHz, SDA truyền bit theo SCL
  
    Start/Stop condition quyết định khi SDA đổi trạng thái lúc SCL cao 

- ```GPIOB->CRL |=  (0xFU << 24) | (0xFU << 28);```  PB6, PB7 AF-OD 50MHz (tốc độ chuyển cạnh tối đa của chân)
- Open-Drain : chân chỉ kéo xuống đất, I2C dùng chung pull-up resistor
- Lưu ý:
  
    Không được cấu hình push-pull; I2C sẽ truyền dữ liệu sai
  
    Phải đảm bảo resistor pull-up ~4.7kΩ cho SCL/SDA
  
    Hầu hết module I2C (LCD, EEPROM, sensor) đã có sẵn pull-up trên board

**1.2 Reset I2C**

- ```I2C1->CR1 = (1U << 15);  // SWRST = 1```  -> reset
- ```I2C1->CR1 = 0; // SWRST = 0``` -> nhả nút 

- Đặt Software Reset (SWRST) để đảm bảo I2C bắt đầu từ trạng thái sạch
- Sau đó nhả reset (0) để bắt đầu cấu hình tiếp
- Lưu ý : Đây là bước quan trọng khi I2C bị treo hoặc khởi tạo lại

**1.3 Cấu hình tần số APB1**

- I2C1->CR2 = 36; // 36MHz = APB1 (PCLK1)
- CR2 chứa tần số clock của bus I2C (PCLK1)
- I2C dùng giá trị này để tính CCR và TRISE
- I2C1 dùng APB1, nhưng tốc độ APB1 thay đổi tùy cấu hình hệ thống
- Lưu ý: Nếu đặt sai, I2C sẽ truyền dữ liệu sai tốc độ, có thể treo bus

**1.4 Cấu hình tốc độ I2C (Fscl = 100 kHz)**
- I2C1->CCR = 180;
  
   CCR = Clock Control Register, xác định độ dài xung SCL
  
   Công thức: CCR = PCLK1 / (2 * Fscl)
  
   CCR = 36,000,000 / (2*100,000) = 180
  
- I2C không bắt buộc là 100 kHz, nhưng 100 kHz là mặc định an toàn và phổ biến nhất
- Lưu ý: Sai công thức = xung SCL quá nhanh/ chậm → LCD/I2C slave không nhận dữ liệu

**1.5 Cấu hình thời gian rise time**
- ```I2C1->TRISE = 37;```
  
   TRISE = thời gian tối đa để SDA/SCL chuyển từ 0→1 theo chuẩn I2C
  
   TRISE = (PCLK1 * 1us) + 1 -> 36MHz * 1us + 1 = 36 +1 = 37
  
- Lưu ý : Sai TRISE → dữ liệu bị lỗi, đặc biệt với tốc độ cao

**1.6 Bật I2C**
- I2C1->CR1 |= (1U << 0);  // Peripheral Enable
- Nếu quên bước này, I2C không hoạt động dù tất cả setup đúng

**1.7 Tóm tắt init I2C trên STM32:**
- Bật clock cho GPIO và I2C trước khi dùng
- Chân SCL/SDA phải AF-OD + pull-up
- Luôn reset I2C trước khi config
- CR2, CCR, TRISE phải tính đúng theo PCLK1
- Bật PE cuối cùng, sau khi đã cấu hình tất cả

## 2. I2C1_Write() 
**2.1 Khởi tạo biến thời gian** 
- ```uint32_t t0;``` :  Dùng để lưu thời điểm bắt đầu chờ các flag I2C
- Kết hợp với msTicks tạo timeout, tránh bị treo nếu slave không phản hồi

**2. Gửi tín hiệu START**

- I2C1->CR1 |= (1U << 8); // Set START = tạo cạnh xuống trên bus
  
    STM32 sẽ kéo SCL/SDA để tạo start condition
  
- t0 = msTicks; 
- while (!(I2C1->SR1 & (1U << 0)))  { if ((msTicks - t0) > I2C_TIMEOUT_MS) return 0; }

    SR1 bit0 = SB (Start Bit): kiểm tra xem start đã được gửi chưa
  
    Nếu slave không phản hồi, vòng while dùng timeout để tránh treo chương trình

**3. Gửi địa chỉ slave**

```c
I2C1->DR = addr; // send address -> clear Start bit
t0 = msTicks;
while (!(I2C1->SR1 & (1U << 1))) // wait ADDR (Address sent)
{
    if ((msTicks - t0) > I2C_TIMEOUT_MS) return 0;
    if (I2C1->SR1 & (1U << 10))  // AF bit (bit 10)
    {
        I2C1->CR1 |= (1U << 9);  // Goi STOP
        (void)I2C1->SR1; 
        (void)I2C1->SR2;        // Ðọc để clear ADDR/AF
        return 0;               // Báo lỗi: slave không ack
    }
}
```

- Gửi địa chỉ I2C của thiết bị slave : ```I2C1->DR = addr;``` -> Tự động clear Start bit
- Xác nhận địa chỉ đã gửi thành công và slave ack : while (!(I2C1->SR1 & (1U << 1)))
  
    Bit 1 của SR1 là ADDR. STM32 gửi START + Address -> Slave ACK -> STM32 set bit ADDR = 1 -> Đã tìm thấy Slave

    Nếu slave không ack, ta gửi STOP và thoát hàm
  
- ```if (I2C1->SR1 & (1U << 10))```

    SR1 bit10 = AF (Acknowledge Failure)
  
    Slave không phản hồi bit 10 set lên 1 , khi đó ta cần clear flag ADDR và báo lỗi 

 
**4. Clear flag ADDR**

- ```(void)I2C1->SR2;```  -> clear ADDR
- Trong STM32, đọc SR1 và SR2 là cách xóa flag ADDR , ta đã đọc SR1 bên trên nên giờ chỉ cần đọc SR2
- Nếu không làm, I2C sẽ không gửi dữ liệu tiếp theo

**5. Gửi dữ liệu byte từng byte**
```c
for (uint8_t i = 0; i < len; i++)
{
    t0 = msTicks; 
    while (!(I2C1->SR1 & (1U << 7))) // TXE: Data Register Empty
    { 
        if ((msTicks - t0) > I2C_TIMEOUT_MS) return 0;
    }
    I2C1->DR = data[i]; // gửi byte tiếp theo
}
``` 

- SR1 bit7 = TXE: kiểm tra xem Data Register rỗng không ? TXE =1 tức là rỗng , có thể gửi dữ liệu mới

- Nếu TXE không set → slave không phản hồi hoặc bus bận → vòng while timeout
- Luôn dùng timeout trong I2C thực tế để tránh treo dự án khi lỗi phần cứng

**6. Kiểm tra kết thúc truyền**

```c
t0 = msTicks; 
while (!(I2C1->SR1 & (1U << 2))) // BTF: Byte Transfer Finished
{
    if ((msTicks - t0) > I2C_TIMEOUT_MS) return 0;
}
```
- Lưu ý: Không được gửi STOP trước khi BTF=1, nếu không sẽ mất byte cuối

**7. Gửi tín hiệu stop**
- I2C1->CR1 |= (1U << 9); // STOP -> kết thúc khung I2C
- Slave sẽ nhận dữ liệu hoàn tất và bus trở về idle
- STOP luôn gửi sau khi BTF=1, nếu không dữ liệu sẽ bị cắt ngang.
->  Hàm trả về 1 nếu gửi thành công, 0 nếu timeout hoặc lỗi ack

**Gải thích : STM32 sẽ kéo SCL/SDA để tạo start condition**
- Mặc định SCL/SDA = HIGH (nhờ pull-up)
- Để tạo Start → phần cứng I2C kéo SDA xuống LOW, trong khi SCL vẫn HIGH

   “Kéo” = điều khiển chân theo kiểu open-drain

    LOW → MCU kéo xuống đất (0)

    HIGH → MCU thả ra, không điều khiển → điện trở pull-up kéo lên

- Tạo Stop bằng  SDA kéo SDA lên HIGHT , trong khi SCL vẫn HIGH

## 3. static void LCD_SendCmd(uint8_t addr, uint8_t cmd)

- PCF8574 không hiểu lệnh LCD , Nó chỉ biết bật/tắt các chân D4–D7, RS, E, BL 
- Vì vậy 1 lệnh LCD → 2 phần 4-bit gửi qua I2C

**3.1 Gửi một lệnh LCD lại phải gửi tới 4 byte I2C**

- LCD dùng giao tiếp 4-bit  : Không gửi cả byte 8-bit một lần
- Tách thành Upper 4 bits và Lower 4 bits (2byte)
- Mỗi 4-bit được gửi qua I2C module PCF8574 → rồi được xuất ra các chân D4..D7 của LCD
- Quan trọng : LCD chỉ chấp nhận dữ liệu khi tín hiệu Enable (E) chuyển từ 1 → 0
- Vì vậy, mỗi nibble cần 2 lần gửi: 1 lần với E=1 , 1 lần với E=0
-> Tổng cộng: Upper (2 byte) + Lower (2 byte) = 4 byte I2C

**3.2 Tách lệnh thành 2 nibble**
```c
uint8_t upper = cmd & 0xF0; 
uint8_t lower = (cmd << 4) & 0xF0;
```
- cmd là lệnh 8-bit của LCD (VD: 0x01 Clear)
- LCD chỉ nhận 4-bit/lần nên chúng ta lấy upper: lấy 4 bit cao , lower: dời 4 bit thấp lên để đưa vào D4..D7
- Tạo 4 gói dữ liệu để mô phỏng xung Enable : Gói 1: upper + E=1 , Gói 2: upper + E=0 ,Gói 3: lower + E=1,Gói 4: lower + E=0.

**3.3 Gửi 4 byte qua I2C**

- I2C1_Write(addr, data, 4); // Dùng hàm I2C đã viết trước đó để gửi
- DelayMs(2); -> LCD là linh kiện chậm , Lệnh clear screen (0x01) cần 1.5–2ms,...

## 4. static void LCD_SendData(uint8_t addr, uint8_t data_)

- addr: địa chỉ I2C của LCD (ví dụ 0x27 hoặc 0x3F).
- data_: byte ký tự cần hiển thị trên LCD.

**4.1 Chuẩn bị dữ liệu 4 nibbles**
```c
uint8_t upper = data_ & 0xF0;
uint8_t lower = (data_ << 4) & 0xF0;
```
**4.2 Tạo các gói dữ liệu I2C cho LCD** 
```c
data[0] = upper | LCD_BACKLIGHT | LCD_ENABLE | LCD_RS;
data[1] = upper | LCD_BACKLIGHT | LCD_RS;
data[2] = lower | LCD_BACKLIGHT | LCD_ENABLE | LCD_RS;
data[3] = lower | LCD_BACKLIGHT | LCD_RS;
```
- LCD_BACKLIGHT: bật đèn nền LCD , LCD_RS: 1 = data (ký tự hiển thị) / 0 = command (lệnh).
- Lưu ý : LCD_RS là bit 0 , LCD_ENABLE bit 2 , LCD_BACKLIGHT bit 3 , upper bit 4-7
 
**4.3 Gửi dữ liệu qua I2C**

- I2C1_Write(addr, data, 4); -> Gửi 4 byte lần lượt để LCD nhận đúng 1 ký tự

**4.4 Delay để LCD kịp xử lý**

- DelayMs(2); 
- LCD cần thời gian xử lý mỗi ký tự (~1.5ms).


## 5. LCD_Init()

**5.1 Delay 50ms trước khi khởi tạo**
- DelayMs(50);
- Khi LCD vừa được cấp nguồn, cần chờ ổn định khoảng 40–50ms trước khi gửi lệnh đầu tiên
- Nếu gửi lệnh quá sớm → LCD không nhận lệnh, màn hình trắng hoặc hiển thị sai

**5.2 Gửi lệnh khởi tạo 0x30 nhiều lần**
```c
LCD_SendCmd(addr, 0x30);
DelayMs(5);
LCD_SendCmd(addr, 0x30);
DelayMs(1);
LCD_SendCmd(addr, 0x30);
DelayMs(10);
```
- Đây là chuỗi khởi tạo chuẩn của LCD HD44780 khi làm việc ở 8-bit mode
- Gửi nhiều lần để LCD chắc chắn nhận lệnh.
- Delay giữa các lệnh: giúp LCD có thời gian xử lý

**5.3 Chuyển LCD sang 4-bit mode**
- LCD_SendCmd(addr, 0x20);
- LCD mặc định ở 8-bit, nhưng ta chỉ dùng 4 dây dữ liệu với I2C

**5.4 Cấu hình LCD: 4-bit, 2 dòng, font 5x8**
- LCD_SendCmd(addr, 0x28); 
- Sai bit này → LCD hiển thị 1 dòng hoặc font sai.

**5.5 Tắt hiển thị trước khi clear** 
- LCD_SendCmd(addr, 0x08);
- 0x08 = Display OFF
- Tắt màn hình trước khi xóa, tránh nháy hoặc hiển thị rác.

**5.6 Xóa toàn bộ màn hình**
- LCD_SendCmd(addr, 0x01); // Clear Display
- DelayMs(2);

**5.7 Cài đặt cursor tự động nhảy**
- LCD_SendCmd(addr, 0x06);
- con trỏ tự động dịch sang phải sau mỗi ký tự 

**5.8 Bật hiển thị, tắt cursor nhấp nháy**
```c
LCD_SendCmd(addr, 0x0C); // 0 0 0 0 1 D C B -> 1100
// Bit 2 (D): Display ON/OFF
// Bit 1 (C): Cursor ON/OFF
// Bit 0 (B): Blink ON/OFF nhay con tro
```
- 0x0C = Display ON, Cursor OFF, Blink OFF
- LCD sẽ hiển thị chữ, không hiện con trỏ nhấp nháy.
- Nếu muốn con trỏ nhấp nháy → set B=1
- Nếu muốn cursor hiện → set C=1

## Lưu ý
- LCD gốc = giao tiếp song song
- I2C không hiểu lệnh LCD
- LCD bản chất có 2 chế độ: 4-bit / 8-bit
- Nhưng khi dùng I2C (module PCF8574), luôn bị ép sang 4-bit
- PCF8574 chỉ làm 1 việc: xuất 8 bit ra chân P0..P7, hoàn toàn không hiểu lệnh

- Set START = tạo cạnh xuống trên bus, không phải chỉ là "bắt đầu"
- Việc đọc SR1 + SR2 để clear ADDR là bắt buộc → nếu không, I2C treo
- SDA và SCL phải có pull-up 4.7kΩ -> Nhiều board không có pull-up, code đúng nhưng thiết bị không chạy

- Thư viện bên trong đã cấu hình sãn #define __IO volatile 
  
   while (!(I2C1->SR1 & 1 << 1));  -> Nếu thiếu volatile → compiler tối ưu hóa → vòng lặp vô hạn
  
   I2C1->CR1 : I2C1 là struct map vào địa chỉ thật của phần cứng , Truy cập qua pointer → ghi trực tiếp vào silicon
  
   -> Câu lệnh C biến thành lệnh thao tác lên phần cứng thật
  
- Mỗi lần đọc SR1 : MCU đọc silicon, không đọc RAM

## Đặc điểm 
- Tiết kiệm chân MCU (2 chân) , Tốc độ thấp hơn SPI
- Hỗ trợ nhiều thiết bị trên 1 bus  
- Phần cứng đơn giản -> Cần quản lý địa chỉ cẩn thận
- Rộng rãi, dễ triển khai -> Dễ nhiễu nếu dây dài


## Video

https://drive.google.com/file/d/1Kctkg2CcfjU1ejqL_DQn6SoKbJJlGDpQ/view?usp=drive_link

