## Mục tiêu

Sử dụng bộ điều khiển PID cho FAN để đưa nhiệt độ mà LM35 đo được tại môi trường về mong muốn


## Giải thích 

**1. Chi tiết hàm float PID_Update(float set, float real)**

- Hàm PID_Update(float set, float real) dùng để tính toán PWM duty (%) dựa trên thuật toán PID
  
   set = giá trị mục tiêu (setpoint) → nhiệt độ mong muốn
  
   real = giá trị thực tế → nhiệt độ đo được (LM35)

**1.1 Thời gian chu kỳ PID – dt**

- ```float dt = 0.5f;```  
- Là khoảng thời gian giữa 2 lần gọi PID (0.5 giây) -> ta phải cấu hình để đúng 0.5s nhảy vào hàm này
  
   Được dùng cho phần I và D để tính tích phân và đạo hàm theo thời gian
  
   Nếu không có dt hoặc dt sai, PID sẽ tính toán sai lệch → dao động, không ổn định
  
   Sau này ta thay bằng thời gian thực lấy từ SysTick

**1.2 Sai số – error**

- Sai số = Mục tiêu – Thực tế
  
   Tất cả các thành phần P, I, D đều hoạt động dựa trên sai số này
  
- Nếu sai số lớn → PWM mạnh
- Nếu sai số nhỏ → PWM giảm dần

**1.3 Vùng chết – Deadband**
- ```if (fabs(error) < 0.2f) error = 0;```
  
    Bỏ qua các sai số rất nhỏ (< 0.2°C)
    
    Nếu không có deadband, “rung PWM” liên tục , quạt/hệ thống sẽ giật liên tục quanh setpoint

**1.4 Thành phần I – Integral**
- integral += error * dt;
  ```c
   if (integral > 100) integral = 100;
   if (integral < -100) integral = -100;
  ```
- Tích phân là phần cộng dồn sai số theo thời gian
- Nó giúp bù sai số còn sót lại khi P không đủ mạnh
- Ví dụ : Nhiệt độ thực là 49.2°C, setpoint = 50°C → error nhỏ → P gần = 0
  
   I sẽ tiếp tục cộng dồn để “đẩy thêm” PWM lên đúng 50°C
  
- Giới hạn integral để tránh tích phân quá lớn → gây overshoot

**1.5 Thành phần D – Derivative**
```c
   float derivative = (error - prev_error) / dt;
   prev_error = error;
```
- D đo tốc độ thay đổi của sai số
  
    Nếu sai số giảm nhanh, hệ thống hiểu là đang tiến gần mục tiêu → giảm PWM sớm để tránh vượt quá
  
    Nếu không có D → dễ bị overshoot (vượt quá setpoint rồi mới giảm lại)

**1.6 Kết hợp P + I + D để tính PWM**
- ```float output = kp*error + ki*integral + kd*derivative;```
- Kết quả = duty cycle (%) để đưa vào PWM

**1.7 Giới hạn đầu ra PWM**
```c
   if (output > 100) output = 100;
   if (output < 0) output = 0;
```
- PWM chỉ hợp lệ trong khoảng 0–100%
- Nếu không giới hạn → PWM âm hoặc >100% → gây lỗi hoặc hư phần cứng

**1.8 Anti-windup – chống tích phân quá lớn**
```c
   if (output == 0 || output == 100)
   integral -= error * dt;
```
- Khi output đang chạm ngưỡng 0 hoặc 100%, không cho integral tiếp tục cộng dồn
- Tránh trường hợp integral quá lớn → khi PWM trở về bình thường sẽ gây dao động mạnh

**1.9 Ví dụ trực quan**

- Giả sử ta đang chạy xe 40 km/h và muốn tăng lên 50 km/h:
- P: Nhấn ga mạnh khi sai số lớn (40 → 50). Khi gần 50 thì giảm ga dần.Nếu chỉ có P → xe dao động nhẹ quanh 50 km/h
  
     Khi xe lên 49 km/h, P hầu như không còn tác dụng (kp * error rất nhỏ)
  
- I sẽ “bù thêm ga” để xe chạm đúng 50 km/h
- D : Khi tốc độ lên nhanh, D sẽ giảm ga sớm để tránh vượt 50 km/h

**1.10 Quy trình chỉnh các hệ số**

- Bước 1 :  Ổn định hệ thống trước : Tắt I và D  ; ki = 0 ,kd = 0 , kp > 0 
- Bước 2 : Bắt đầu từ kp rất nhỏ , Tăng dần kp cho đến khi :
  
   Hệ thống vào dao động lặp lại nhưng chưa nổ tung , có biên độ dao động ổn định.
  
- Tại một giá trị nào đó, hệ thống bắt đầu: 49.0° → 51.0° → 49.0° → 51.0° → lặp lại mãi

   duty : 42% → 60% → 42% → 60% → 42% → ...

   Thời điểm này lấy : Kp_crit (Kp tới hạn) ,Tu = chu kỳ dao động (thời gian giữa 2 đỉnh hoặc đo nhiều chu kì liên tiếp , lấy trung bình)
  
- Bước 3 — Dùng công thức Ziegler–Nichols :
   
     P : Kp = 0.50·Kcrit
  
     PI: Kp = 0.45.Kcrit , Ki = 1.2·Kp / Tu
  
     PID : Kp = 0.60.Kcrit , Ki = 2·Kp / Tu , Kd = Kp·Tu / 8
  
- Tiếp tục điều chỉnh :
  
    Giảm dao động? → tăng Kd
  
    Lên setpoint chậm? → tăng Ki
  
    Quá nhạy, phản ứng giật? → giảm Kp



## Ảnh chụp 

