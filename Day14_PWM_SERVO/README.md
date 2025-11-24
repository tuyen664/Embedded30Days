# STM32 Điều Khiển Servo Qua UART

## Mục tiêu 

- **Điều khiển servo** sử dụng **vi điều khiển STM32F1** với **PWM TIM4** và giao tiếp **UART1**.
- Gửi lệnh điều khiển góc servo (0–180°) trên Tera Term qua UART, và servo sẽ xoay đến góc tương ứng.
- Sử dụng SysTick để tạo **delay theo ms**

## Kết nối phần cứng 

- PB9 : TIM4_CH4 (PWM) -> Nối với chân tín hiệu servo (Dây màu vàng) 
- PA9 : UART1_TX  -> Nối với USB-UART/PC 
- PA10 : UART1_RX -> Nối với USB-UART/PC
- Servo dùng nguồn 5V ngoài
- GND chung

## Giải thích 

- Servo thường hoạt động trong khoảng 500µs → 2500µs.
- Trong code sử dụng 600µs → 2400µs để tránh rung hoặc chạm biên (nhiều servo bị không ổn định ở mức giới hạn).

**1. Cấu hình PWM cho PB9 (PWM TIM4)**

- Bật clock cho GPIOB và TIM4 , bật AFIO
- Cấu hình PB9 là AF Push-Pull ,  tốc độ 50MHz
- TIM4->PSC = 72-1; // 1 tick = 1µs (với TIM4 = 72MHz)-> Servo cần 600 tick -> 2400 tick (TIM4->CCR4)
- TIM4->ARR = 20000-1; // servo hoạt động ở chu kì 20ms = 50hz
- TIM4->CCR4 = 1500; // Mặc định sau khi reset đưa servo về vị trí trung tâm (khoảng 90°)

**2. Hàm chuyển từ Góc cần quay sang số tick cần điều khiển trong TIM4->CCR4** 

- Ở đây ta điều khiển trực tiếp qua số tick ở TIM4->CCR4  (không cần đến phần trăm duty)
- pulse = SERVO_MIN_US + ((SERVO_MAX_US - SERVO_MIN_US) * angle) / 180U; // (600 -> 2400) tương ứng (0 -180 độ)
- Cập nhật luôn vào TIM4->CCR4 = pulse ;
- TIM4->EGR |= 1; // thay đổi ngay lập tức
- Khi ta thay đổi CCR4 và set EGR |= 1,timer cần ít nhất một chu kỳ để phát ra xung PWM mới.
- Delay 10 ms đảm bảo PWM ổn định , tránh cập nhật quá nhanh.

**3. Vòng lặp chính** 
 
- Tạo mảng **char buffer[32];** để lưu lệnh nhận được từ Tera Term qua UART
- Biến c đọc từng ký tự nhận về , biến i là idex của buffer  

- Vòng lặp while(1) :
- c đọc từng ký tự nhận về từ USART1->DR 
- Nếu c đọc đến '\r' hoặc '\n' → kết thúc chuỗi
- khi này ta cần kiểm tra i : if (i == 0) continue;// Xem chuỗi có rỗng không
- Sau khi nhập xong thì ghi '\0' vào cuối chuỗi **buffer[i] = '\0';**

- Nếu c chưa đọc đến '\r' hoặc '\n' và kí tự nhận được là số từ '0' đến '9' và  i < sizeof(buffer) - 1 thì ta tiếp tục khi c vào mảng buffer
- Ta dùng điều kiện i < sizeof(buffer) - 1) để Chống tràn bộ đệm buffer (chỉ có 32byte , người dùng có thể nhập rất nhiều số)
- Để dành 1 ô trống để ghi kí tự '\0' cuối chuỗi

- Nếu kí tự c khác số, in ra màn hình **Invalid input. Enter 0-180.**, cho i về 0 và thoát

- Sau khi gửi nhận dữ liệu vào buffer xong , ta đổi chuỗi sang số : int angle = atoi(buffer);
- Giới hạn lại góc angle tại 0-180
- Gửi lệnh thực thi qua TIM4->CCR4 bằng hàm **Servo_SetAngle((uint8_t)angle);**
- Delay 1 khoảng thời gian để servo có thời gian quay 
- In thông báo góc đã quay của servo lên màn hình. 

Ảnh chụp

