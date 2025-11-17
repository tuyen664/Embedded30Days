# STM32 Mini Robot – Servo + ADC + UART

- Điều khiển servo bằng PWM TIM4,
- Đọc cảm biến qua ADC + DMA
- Chuyển chế độ **Auto / Manual** thông minh
- Mục tiêu: Tạo hệ thống phản hồi theo cảm biến và cho phép can thiệp điều khiển thủ công

## Tính năng

- Điều khiển servo 0–180° qua UART (Tera Term)
- Tự động xoay servo dựa trên 3 cảm biến analog (PA0 / PA1 / PA2)
- Chế độ Manual → trả về Auto sau **5 giây** không nhập lệnh
- ADC chạy liên tục ở chế độ scan mode + DMA
- Servo ở TIM4_CH4 (PB9)

## Giải thích

### Giao tiếp UART (Manual Mode) 

- Gửi góc servo trên Tera Term , mỗi khi nhấn enter , servoo đổi theo góc nhận được
- Sau 5 giây không nhận dữ liệu → tự chuyển về Auto Mode.

### Auto Mode (Dựa vào cảm biến ADC)

- Đọc điện áp từ 3 channel ADC: CH0 → PA0 → left , CH1 → PA1 → mid (không dùng) , CH2 → PA2 → right
- Logic điều khiển : 
- if (left > right + 0.3f) servo_angle = 180; 
- else if (right > left + 0.3f) servo_angle = 0; 
- else  servo_angle = 90;
                                   
### Phần cứng và phần mềm

- STM32F103C8T6
- Keil C / STM32CubeIDE
- Nạp bằng ST-Link 
- Tera Term (9600 baud, 8N1)
- Servo PWM  : PB9 (TIM4_CH4)
- UART TX / RX : PA9 / PA10
- ADC–CH0 / CH1/CH2 -> PA0 / PA1 / PA2  

### Quy trình chạy

- Nạp firmware vào STM32           
- Mở Tera Term → 9600 baud , chọn đúng COM
- Gửi góc servo (0–180) để điều khiển Manual
- Không gửi gì 5 giây → hệ thống tự chuyển về Auto Mode


## Ảnh chụp 

