## Mục tiêu

Cấu hình PA1 làm  **nút nhấn** thay đổi trạng thái Led PC13 , biết cách **chống dội** khi nhấn nút 

## Phần cứng sử dụng

STM32F103 và Nút nhấn đấu PA1


## Giải thích code hoạt động
**1. Cấu hình GPIO**

- Cấu hình PC13 là chân nháy led , output push-pull 50MHz

- Cấu hình PA1 là  **Input Pull-up** : PA1 được dùng làm ngõ vào và được kéo lên
mức **logic cao (1)** bên trong vi điều khiển bằng điện trở nội bộ.

- khi nhấn nút : 1->0 (PA1 nối lên 3.3V qua pull-up -> xuống đất) , thả tay : 0->1 

- Không dùng Floating (thả nổi ) vì không có điện trỞ kéo lên hay kéo xuống
  
  PA1 bị nhiễu , dao động ngẫu nhiên giữa 0 và 1 -> vi điều khiển đọc sai trạng thái.
   
  Nên dùng pull-down hoặc pull-up.

- Cấu hình SysTick cứ 1ms là kích hoạt COUNTFLAG 

**2. Hoạt động hàm chính**
 - Khởi tạo :
   ```c
      GPIO_Config();
      GPIO_ResetBits(GPIOC, GPIO_Pin_13);  // LED sáng (active low)
   ```

 - Kiểm tra nút nhấn : ``` if (0U == GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1)) ```
   
   Nếu PA1 ở mức 0, nghĩa là nút đang được nhấn.

 - Chống dội (Debounce): ``` Delay_Ms(100U);```
   
   Khi nhấn nút thật, tiếp điểm cơ học bị rung (bounce), tạo ra các 
   xung 0/1 nhanh trong vài mili-giây.
   
   Lệnh Delay_Ms(100) giúp chờ tín hiệu ổn định, tránh đọc sai nhiều lần.

 - check lại xem nút có đúng đang nhấn không : 
  if (0U == GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1))

   Đảm bảo nút thực sự đang được nhấn, không phải nhiễu.

- Đợi cho đến khi nút được nhả ra : 0-> 1
 ``` while (0U == GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1)); ```

- Tăng biến đếm lên , nếu là chẵn thì cho led on , nếu lẻ thì led off.

## Debug

https://drive.google.com/file/d/1-e16S3Tl78Z0v151_nM45kagH3iIcHg1/view?usp=drive_link

