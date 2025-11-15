# STM32 Mini Robot – Servo + ADC + UART

Điều khiển servo bằng PWM TIM4, đọc cảm biến qua ADC + DMA, và chuyển chế độ **Auto / Manual**thông qua **UART**

## Tính năng

- Điều khiển servo 0–180° qua UART (Tera Term)
- Tự động xoay servo dựa trên 3 cảm biến analog (PA0 / PA1 / PA2)
- Chế độ Manual → trả về Auto sau **5 giây** không nhập lệnh
- ADC chạy liên tục chuyển sữ liệu bằng DMA
- Servo ở TIM4_CH4 (PB9)

## Giải thích

### Giao tiếp UART (Manual Mode) 

- Gửi góc servo trên Tera Term , mỗi khi nhấn enter , servoo đổi theo góc nhận được
- Sau 5 giây không nhận dữ liệu → tự chuyển về Auto Mode.

### Auto Mode (Dựa vào cảm biến ADC)

- Đọc điện áp từ 3 channel ADC: CH0 → PA0 → left , CH1 → PA1 → mid (không dùng) , CH2 → PA2 → right
- logic điều khiển : ![hinh1](./hinh/hinh1.png)

     



## Ảnh chụp 

