## Mục tiêu

- Biết cấu hình ngắt ngoài **EXTI** cho PA1 làm nút nhấn điều khiển Led PC13 thay đổi trạng thái.


## Giải thích

**1. Cấu hình GPIO**

 - Bật clock cho GPIOA ,GPIOC , đặc biệt là **Alternate Function**  
  ``` RCC->APB2ENR |= (1 << 2) | (1 << 4) | (1 << 0);```
 - Cấu hình PC13 Là **output push-pull 2Mhz**
 - Cấu hình PA1 là Input pull-up (cho nút nhấn) , kéo PA1 lên pull-up
   
    Output : MCU tạo tín hiệu ra ngoài (dùng cho LED, motor, relay, v.v)
   
    Input  : MCU nhận tín hiệu từ ngoài (Nút nhấn, cảm biến, UART RX, v.v)
   
 - ```GPIOA->ODR |=  (1 << 1);```    

**2. Cấu hình EXTI cho PA1**

 - Reset EXTI1 : ```AFIO->EXTICR[0] &= ~(0xF << 4); ```
 - AFIO có bộ thanh ghi EXTICR[0...3] để chọn port nguồn cho các EXTI lines
   
    EXTICR[0] chứa cấu hình cho EXTI0, EXTI1, EXTI2, EXTI3 , mỗi EXTI chiếm 4 bit
   
   -> EXTI1 bits [7:4] của EXTICR[0]
   
 - Kết nối EXTI1 với PA1 : ```AFIO->EXTICR[0] |=  (0x0 << 4);  // chon port A (0b0000) ```
   
   -> 0b0001 = port B , 0b0010 = port C ,...
   
 **Ví dụ muốn nối EXTI1 với PB1** : AFIO->EXTICR[0] |= (0x1 << 4); // chọn Port B cho EXTI1
 
 -  EXTI->IMR  |= (1 << 1);   // Unmask line 1 -> cho phep line 1 phat ngat
 + EXTI->IMR = Interrupt Mask Register (Thanh ghi mặt nạ ngắt)
 + bit 0 -> 18 : Mỗi bit tương ứng một line EXTI (0–18)
 + bit = 1 : cho phép line đó sinh ngắt
 + bit = 0 : chặn line đó , tức có sự kiện EXTI nhưng không báo ngắt lên NVIC 

 - EXTI->FTSR |= (1 << 1);   // Ngắt cạnh xuống
 + Bit = 1 Kích hoạt ngắt cạnh xuống ( khi bấm nút PA1 pull-up từ 1-> 0 -> sinh ngắt )
 + Bit = 0 không kích hoạt ngắt
 
 - EXTI->RTSR |= (1 << 1);   // Rising trigger
 + khi ta cấu hình PA1 là pull-up nên khi bấm chưa kích hoạt ngắt , mà phải thả nút thì mới
kích hoạt (1->0->1)

 - Quan trọng : **Bật ngắt trong NVIC** :  NVIC_EnableIRQ(EXTI1_IRQn);// EXTI1 -> ngắt line 1

 - NVIC :Bộ điều khiển ngắt trung tâm quản lý ngắt từ EXTI, USART, Timer, ADC...và ưu tiên
  xử lý chúng

**3. Hàm xử lý ngắt**

 - khi ta nhấn PA1 thì kích hoạt ngắt , CPU sẽ **tự động** nhảy vào hàm 
  **void EXTI1_IRQHandler(void)** - đây là tên mặc định đã cấu hình sẵn
  trong bảng vector ngắt của STM32 
 
 - if (EXTI->PR & (1 << 1)) // kiểm tra xem ngắt có thực sự đến từ line 1 
 + EXTI->PR = Pending Register, chứa cờ báo ngắt đang xử lý
 + (1 << 1) = bit số 1 → EXTI line 1 (PA1)

 - Nếu đúng xảy ra ngắt thì đảo Led : GPIOC->ODR ^= (1 << 13);
 - Xóa cờ ngắt : EXTI->PR = (1 << 1);
 + **Trong STM32, để xóa cờ ngắt EXTI, ta phải ghi giá trị 1 vào chính bit đó.(không phải 0)**
 + chú ý quan trọng : không nên dùng EXTI->PR |= (1 << 1); vì khi đó ta ghi 1 vào các bit khác
 có thể xóa nhầm khi dùng nhiều ngắt cùng lúc


## Video

https://drive.google.com/file/d/1UX4Q1Op5CziT_kjOUEH1PbcvqfQ7NQPe/view?usp=sharing






