## Mục tiêu

 - Thiết kế ngắt EXTI1 cho nút nhấn PA1
 - Thiết kế ngắt TIM2 để điều khiển tốc độ nháy của LED
 - Thiết kế ngắt TIM3 thay hàm Delay_ms để chống dội cho nút nhấn 

## Giải thích

**1. Cấu hình GPIOA, GPIOC, TIM2 , TIM3 , EXTI1**

 - Cấu hình PC13 là output push-pull 50MHz
 - Cấu hình PA1 là Input pull-up để nối với nút nhấn
 - Cấu hình TIM2 , TIM3 với :
  + TIM2->PSC = 7199;            // 1 tick = 0.1ms (72MHz / 7200 = 10kHz)
  + TIM2->ARR = 10U*arr;         // số arr chính là số ms
  + Lưu ý : Thanh ghi TIM2->PSC chỉ có 16 bit (0-65535) nên nếu cho TIM2->PSC = 71999; là lỗi    
 - Cấu hình EXTI1 cho PA1

**2. Quá trình hoạt động**

- Ban đầu khi nhấn nút Reset , LED trên chân PC13 nháy đều với chu kì 1s
- Khi ta nhấn PA1 , ngay lập tức kích hoạt ngắt EXTI1 , CPU chạy vào ngắt **void EXTI1_IRQHandler(void)**
- Sau khi kiểm tra lại if (EXTI->PR & (1<<1)) ta đóng ngắt EXTI1 tránh nhiễu gây ngắt liên tục

- Biến speed_mode = !speed_mode; (ban đầu speed_mode = 0 , sau lần 1 nhấn là 1, lần 2 là 0,... )
- khi speed_mode = 1 thì LED nháy chu kì là 500ms (khi ghi câu lệnh TIM2->ARR = 5000; ta phải nhân 10
lên vì 1tick / 0.1ms)
- Khi speed_mode = 0 thì LED nháy chu kì là 1s
- Sau đó ta phải lập tức đưa TIM2->CNT = 0; vì có thể count đang đếm trong chu kì mặc định
- Thêm TIM2->EGR |= (1<<0);   để cập nhật ngay lập tức

- Ta chống dội bằng TIM3 : TIM3_Config(50); 
- Sau 50ms , CPU đi vào **void TIM3_IRQHandler(void)**
- Trong này ta bật lại ngắt EXTI1 , reset lại TIM3->SR &= ~(1<<0); và stop TIM3 ,
 cho lần chống dội tiếp theo

## Ảnh chụp

