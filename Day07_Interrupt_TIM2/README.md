## Mục tiêu 
 
 - Biết dùng Timer ngắt , mỗi 0.5s nháy LED PC13 1 lần

## Giải thích 

 **1. Cấu hình TIM2**
  - Bật Clock cho TIM2 : ```RCC->APB1ENR |= (1<<0);``` (TIM2 nằm trên bus APB1 bit 0;)
  - Đặt lại CR1 : ```TIM2->CR1 = 0;``` -> Đảm bảo Timer ở trạng thái mặc định
  - Reset lại bộ đếm hiện tại về 0 : TIM2->CNT = 0;
    
     -> Tránh Timer đang chạy từ cấu hình cũ

  - ```TIM2->PSC = 7199;``` Bộ chia tần số : Clock Timer = 72MHz / 7200 = 10KHz (10KHz/s) -> 0.1ms/ Tick
    
      APB1 bus clock (PCLK1) có tần số mặc định 36 MHz . Tuy nhiên Timer clock (TIM2–TIM7) lại chạy với
      tần số là 72MHz do nhân đôi **khi PPRE1 (prescaler của APB1) ≠ 1**

  - ```TIM2->ARR = 5000-1;```  TIMER sẽ đếm từ 0 đến 4999 rồi CPU nhảy vào ngắt.
    
      Với 10KHz/s thì 5000tick = 0.5s
    
      Ép TIMER cập nhật cấu hình mới ngay lập tức : ```TIM2->EGR |= (1 << 0);```
 
  - Cho phép TIMER sinh ra ngắt : ```TIM2->DIER |= (1<<0);``` (bit 0  là UIE- update interrupt enable)
    
     Nếu không bật UIE, Timer vẫn đếm nhưng không tạo ngắt.

  - Bật Counter Enable (CEN) ``` TIM2->CR1 |= (1<<0);```
    
  - Bật ngắt TIM2 trong NVIC :``` NVIC_EnableIRQ(TIM2_IRQn);```

**2. Khi nhấn nút Reset**
  
  - Mỗi 0.5s là CPU lại nhảy vào ngắt TIM2 , cụ thể là hàm ```void TIM2_IRQHandler(void)``` 
  - Mỗi khi đếm tràn , bit 0 (UIF-Update Interrupt Flag) của thanh ghi SR (Status Register) được set lên 1
  - kiểm tra bit 0 (UIF) xem có bằng 1 : ```if (TIM2->SR & (1 << 0)) ;```
  - Xóa cờ ngắt sau khi xử lý xong để lần sau Timer tràn thì kích hoạt lại.
    
    TIM2->SR &= ~(1 << 0);      // Clear flag
    
    UIF không tự xóa khi CPU đọc, phải ghi 0 vào bit này để clear
  
  - Toggle Led : ```GPIOC-> ODR ^= (1 << 13);``` 


## Video

https://drive.google.com/file/d/18Ph7lqmNJ-cUMR9tILX5DuqIft54AYip/view?usp=drive_link

