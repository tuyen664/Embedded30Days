## Mục tiêu 

 - Dùng ngắt **EXTI** tại PA1 để cập nhật trạng thái nút đã nhấn chưa và thời gian để chống dội
 - Dùng ngắt **SysTick** để xử lý **chống dội** và đảo LED PC13 khi cập nhật kết quả từ EXTI.

## Giải thích 

**1. Cấu Hình GPIO Và AFIO**
  - Bật Clock cho GPIOA , GPIOC , AFIO
  - Cấu hình PC13 là output push-pull 2MHz
  - Cấu hình PA1 là Input pull-up

**2. Cấu hình EXTI**
  - Ta dùng PA1 là nút nhấn nên -> dùng line 1 -> ( AFIO->EXTICR[0] )
  - Chọn port A cho line 1 nên ->  AFIO->EXTICR[0] |=  (0x0 << 4);
  - Bật bit 1 của IMR để cho phép ngắt ở line 1 : EXTI->IMR  |= (1 << 1);
  - kích hoạt ngắt khi có cạnh xuống : EXTI->FTSR |= (1 << 1);  
  - Bật ngắt EXTI1 trong NVIC : NVIC_EnableIRQ(EXTI1_IRQn);

**3. Cấu hình sysTick**
  - Ta muốn mỗi 1ms , ngắt kích hoạt 1 lần nên : SysTick->LOAD = 72000 - 1;
  - khi VAL đếm từ LOAD -> 0 sẽ reset lại : SysTick->VAL  = 0;
  - Bật sysTick (bit 0) , bật ngắt sysTick(bit 1) , **chọn nguồn clock** HCLK = 72MHz (bit 2)
  + SysTick->CTRL = 7;    

**4. Khi nhấn PA1**
  - Khi nhấn PA1 , ngay lập tức kích hoạt ngắt EXTI , CPU nhảy vào void EXTI1_IRQHandler(void)
  - Kiểm tra lại cờ ngắt ở line 1 (đọc bit 1 của PR) , nếu có thì thực thi chương trình :
  + Sau khi nhảy vào, cờ pending PR phải được xóa thủ công để cho phép ngắt lần sau
  + xóa cờ ngắt bằng cách **ghi 1** vào bit 1 của thanh ghi PR
  + cập nhật buttonPressed  = 1; đánh dấu là đã nhấn 
  + debounceTime = 50;         // Chong doi 50ms
  + Tắt tạm ngắt EXTI1 trong NVIC tránh nhiễu lúc nhấn nút gây ra kích hoạt ngắt liên tục
   
  -SysTick ngắt mỗi 1ms, kiểm tra biến debounceTime đã cập nhật chưa , nếu có giảm biến 
   debounceTime dần về 0 (sau khoảng 50ms)
  - kiểm tra if(debounceTime == 0 && buttonPressed ) 
  + Biến **buttonPressed rất cần thiết** để xác định rằng ngắt EXTI thực sự đã xảy ra, đặc biệt khi
  có nhiều nguồn ngắt (Biết ngắt ở chỗ nào)hoặc mở rộng hệ thống sau này.
  - Nếu !(GPIOA->IDR & (1 << 1)) → nghĩa là nút vẫn đang được nhấn → đảo trạng thái LED.
  - Đặt buttonPressed = 0; để xóa cờ, và bật lại ngắt EXTI1 trong NVIC.


## Ảnh chụp 

